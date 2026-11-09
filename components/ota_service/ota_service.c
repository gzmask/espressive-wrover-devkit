#include "ota_service.h"
#include "sdkconfig.h"
#if CONFIG_OTA_SERVICE_ENABLE

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_heap_caps.h"
#include "esp_https_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include "ota_policy.h"
#include "wifi_service.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#if !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#error "OTA must use the separately approved rollback bootloader"
#endif
extern const uint8_t token_start[] asm("_binary_node_ota_token_start");
extern const uint32_t token_length asm("node_ota_token_length");
extern const uint8_t cert_start[] asm("_binary_node_ota_cert_start");
extern const uint8_t cert_end[] asm("_binary_node_ota_cert_end");
extern const uint8_t key_start[] asm("_binary_node_ota_key_start");
extern const uint8_t key_end[] asm("_binary_node_ota_key_end");

static const char *TAG = "ota_service";
static bool (*health_check)(void);
static SemaphoreHandle_t control;
static bool started;
static bool reboot_requested;
static bool commit_locked;
static int64_t reboot_at;
static char image_hash[65];

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static bool online(void)
{
    wifi_link_snapshot_t link;
    return wifi_service_get_link_snapshot(&link) && link.phase == WIFI_LINK_ONLINE;
}

static bool pending(void)
{
    esp_ota_img_states_t state;
    return esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
           state == ESP_OTA_IMG_PENDING_VERIFY;
}

static bool confirmed(void)
{
    esp_ota_img_states_t state;
    return esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
           state == ESP_OTA_IMG_VALID;
}

static esp_err_t respond(httpd_req_t *req, const char *status, const char *body)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, body);
}

static bool authenticated(httpd_req_t *req)
{
    char header[72];
    size_t length = httpd_req_get_hdr_value_len(req, "Authorization");
    return length == 71 &&
           httpd_req_get_hdr_value_str(req, "Authorization", header, sizeof(header)) == ESP_OK &&
           ota_authorized(header, length, token_start);
}

static esp_err_t status_handler(httpd_req_t *req)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    char body[384];
    cJSON *json = cJSON_CreateObject();
    bool ok = json != NULL && cJSON_AddStringToObject(json, "version", esp_app_get_description()->version) &&
              cJSON_AddStringToObject(json, "image_sha256", image_hash) &&
              cJSON_AddNumberToObject(json, "slot_address", running->address) &&
              cJSON_AddBoolToObject(json, "online", online()) &&
              cJSON_AddBoolToObject(json, "healthy", health_check()) &&
              cJSON_AddBoolToObject(json, "pending_verify", pending()) &&
              cJSON_AddBoolToObject(json, "confirmed", confirmed()) &&
              cJSON_PrintPreallocated(json, body, sizeof(body), false);
    cJSON_Delete(json);
    return respond(req, ok ? "200 OK" : "500 Internal Server Error",
                   ok ? body : "{\"error\":\"status unavailable\"}");
}

static bool layout_valid(void)
{
    const esp_partition_t *a = esp_partition_find_first(ESP_PARTITION_TYPE_APP, 0x10, NULL);
    const esp_partition_t *b = esp_partition_find_first(ESP_PARTITION_TYPE_APP, 0x11, NULL);
    const esp_partition_t *data =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
    if (!a || !b || !data || a->address != 0x10000 || b->address != 0x160000 || a->size != OTA_SLOT_SIZE ||
        b->size != OTA_SLOT_SIZE || data->address != 0xc000 || data->size != 0x2000)
        return false;
    unsigned count = 0;
    esp_partition_iterator_t it =
        esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
    while (it) {
        ++count;
        it = esp_partition_next(it);
    }
    return count == 1;
}

static bool image_header_valid(const uint8_t *buffer)
{
    esp_image_header_t image;
    esp_app_desc_t app;
    memcpy(&image, buffer, sizeof(image));
    memcpy(&app, buffer + sizeof(image) + sizeof(esp_image_segment_header_t), sizeof(app));
    return image.magic == ESP_IMAGE_HEADER_MAGIC && image.chip_id == ESP_CHIP_ID_ESP32 &&
           image.hash_appended == 1 && image.segment_count > 0 && image.segment_count <= 16 &&
           app.magic_word == ESP_APP_DESC_MAGIC_WORD &&
           memchr(app.project_name, 0, sizeof(app.project_name)) != NULL &&
           memchr(app.version, 0, sizeof(app.version)) != NULL &&
           memchr(app.idf_ver, 0, sizeof(app.idf_ver)) != NULL &&
           strcmp(app.project_name, esp_app_get_description()->project_name) == 0 &&
           strcmp(app.idf_ver, esp_get_idf_version()) == 0;
}

/* Single HTTP task owns upload state. Never writes the running slot or a
 * bootloader/table/data partition. Boot selection changes only after both
 * supplied SHA-256 and SDK image validation have succeeded. */
static esp_err_t upload_handler(httpd_req_t *req)
{
    if (!authenticated(req)) {
        (void)respond(req, "401 Unauthorized", "{\"error\":\"unauthorized\"}");
        return ESP_FAIL; /* close without interpreting an unauthenticated body */
    }
    char expected_hex[65], content_type[40], requested_target[9];
    uint8_t expected[32], actual[32];
    const size_t prefix_size =
        sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);
    if (strcmp(req->uri, "/ota") != 0 || httpd_req_get_hdr_value_len(req, "Transfer-Encoding") != 0 ||
        httpd_req_get_hdr_value_str(req, "X-OTA-Target", requested_target, sizeof(requested_target)) !=
            ESP_OK ||
        httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) != ESP_OK ||
        strcmp(content_type, "application/octet-stream") != 0 ||
        httpd_req_get_hdr_value_len(req, "X-Image-SHA256") != 64 ||
        httpd_req_get_hdr_value_str(req, "X-Image-SHA256", expected_hex, sizeof(expected_hex)) != ESP_OK ||
        !ota_decode_sha256(expected_hex, 64, expected) || req->content_len < prefix_size ||
        req->content_len > OTA_SLOT_SIZE) {
        (void)respond(req, "400 Bad Request", "{\"error\":\"invalid upload headers/length\"}");
        return ESP_FAIL;
    }
    xSemaphoreTake(control, portMAX_DELAY);
    bool busy = reboot_requested || commit_locked || !confirmed();
    xSemaphoreGive(control);
    if (busy || !online() || !health_check()) {
        (void)respond(req, "409 Conflict", "{\"error\":\"not ready\"}");
        return ESP_FAIL;
    }
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    if (!target || target->type != ESP_PARTITION_TYPE_APP ||
        !ota_target_allowed(running->address, target->address, target->size, target->subtype) ||
        strcmp(requested_target, target->address == 0x10000 ? "0x10000" : "0x160000") != 0) {
        (void)respond(req, "409 Conflict", "{\"error\":\"no safe inactive slot\"}");
        return ESP_FAIL;
    }

    uint8_t *buffer = heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buffer) {
        (void)respond(req, "503 Service Unavailable", "{\"error\":\"memory\"}");
        return ESP_FAIL;
    }
    esp_ota_handle_t handle = 0;
    bool active = false;
    size_t received = 0, prefix = 0;
    esp_err_t error = ESP_FAIL;
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    int64_t deadline = now_ms() + 120000;
    while (prefix < prefix_size && now_ms() < deadline) {
        int n = httpd_req_recv(req, (char *)buffer + prefix, prefix_size - prefix);
        if (n <= 0)
            goto cleanup;
        prefix += (size_t)n;
    }
    if (prefix != prefix_size || !image_header_valid(buffer))
        goto cleanup;
    if (mbedtls_sha256_starts(&sha, 0) != 0)
        goto cleanup;
    if (esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &handle) != ESP_OK)
        goto cleanup;
    active = true;
    while (received < req->content_len && now_ms() < deadline) {
        size_t count = prefix;
        if (prefix == 0) {
            size_t remaining = req->content_len - received;
            int n = httpd_req_recv(req, (char *)buffer, remaining < 4096 ? remaining : 4096);
            if (n <= 0)
                goto cleanup;
            count = (size_t)n;
        }
        if (!online() || mbedtls_sha256_update(&sha, buffer, count) != 0 ||
            esp_ota_write(handle, buffer, count) != ESP_OK)
            goto cleanup;
        received += count;
        prefix = 0;
        vTaskDelay(1); /* let LCD/heartbeat run between flash operations */
    }
    if (received != req->content_len || mbedtls_sha256_finish(&sha, actual) != 0 ||
        memcmp(actual, expected, sizeof(actual)) != 0)
        goto cleanup;
    error = esp_ota_end(handle);
    active = false; /* end consumes handle even on validation failure */
    if (error != ESP_OK)
        goto cleanup;
    /* A failed metadata commit may have partially changed boot selection.
     * Refuse further uploads until reset, rather than retrying over it. */
    xSemaphoreTake(control, portMAX_DELAY);
    commit_locked = true;
    xSemaphoreGive(control);
    error = esp_ota_set_boot_partition(target);
    if (error == ESP_OK) {
        ESP_LOGI(TAG, "validated application; next_slot=0x%" PRIx32 " bytes=%u", target->address,
                 (unsigned)received);
    }
cleanup:
    if (active)
        (void)esp_ota_abort(handle);
    mbedtls_sha256_free(&sha);
    free(buffer);
    if (error != ESP_OK) {
        (void)respond(req, "400 Bad Request",
                      "{\"error\":\"upload rejected; boot selection unchanged unless commit failed\"}");
        return ESP_FAIL;
    }
    esp_err_t sent = respond(req, "200 OK", "{\"accepted\":true,\"rebooting\":true}");
    xSemaphoreTake(control, portMAX_DELAY);
    reboot_requested = true;
    reboot_at = now_ms() + 1000;
    xSemaphoreGive(control);
    return sent;
}

static esp_err_t start_server(httpd_handle_t *server)
{
    httpd_ssl_config_t cfg = HTTPD_SSL_CONFIG_DEFAULT();
    cfg.port_secure = 8443;
    cfg.httpd.max_open_sockets = 2;
    cfg.httpd.max_uri_handlers = 3;
    cfg.httpd.task_priority = 2;
    cfg.tls_handshake_timeout_ms = 5000;
    cfg.servercert = cert_start;
    cfg.servercert_len = (size_t)(cert_end - cert_start);
    cfg.prvtkey_pem = key_start;
    cfg.prvtkey_len = (size_t)(key_end - key_start);
    esp_err_t error = httpd_ssl_start(server, &cfg);
    if (error != ESP_OK)
        return error;
    const httpd_uri_t endpoints[] = {
        {.uri = "/health", .method = HTTP_GET, .handler = status_handler},
        {.uri = "/status", .method = HTTP_GET, .handler = status_handler},
        {.uri = "/ota", .method = HTTP_POST, .handler = upload_handler},
    };
    for (size_t i = 0; i < sizeof(endpoints) / sizeof(endpoints[0]); ++i) {
        error = httpd_register_uri_handler(*server, &endpoints[i]);
        if (error != ESP_OK) {
            (void)httpd_ssl_stop(*server);
            *server = NULL;
            return error;
        }
    }
    ESP_LOGI(TAG, "authenticated HTTPS OTA ready; port=8443; no NVS writes");
    return ESP_OK;
}

static void ota_task(void *argument)
{
    (void)argument;
    httpd_handle_t server = NULL;
    bool attempted = false;
    bool confirmation_attempted = false;
    int64_t deadline = now_ms() + 120000;
    int64_t healthy_since = 0;
    for (;;) {
        int64_t now = now_ms();
        if (!attempted && online() && health_check()) {
            attempted = true;
            if (start_server(&server) != ESP_OK)
                ESP_LOGE(TAG, "HTTPS startup failed");
        }
        xSemaphoreTake(control, portMAX_DELAY);
        if (reboot_requested && now >= reboot_at)
            esp_restart();
        if (pending()) {
            if (server && online() && health_check()) {
                if (healthy_since == 0)
                    healthy_since = now;
                if (!confirmation_attempted && now - healthy_since >= 30000) {
                    confirmation_attempted = true;
                    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
                        ESP_LOGI(TAG, "pending image confirmed after30s healthy service");
                }
            } else {
                healthy_since = 0;
            }
            if (now >= deadline && pending()) {
                ESP_LOGE(TAG, "unconfirmed boot deadline; restarting for bootloader rollback");
                esp_restart();
            }
        }
        xSemaphoreGive(control);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t ota_service_start(bool (*healthy)(void))
{
    if (started || !healthy || token_length != 32 || !layout_valid())
        return ESP_ERR_INVALID_STATE;
    const esp_partition_t *running = esp_ota_get_running_partition();
    uint8_t digest[32];
    if (esp_partition_get_sha256(running, digest) != ESP_OK)
        return ESP_ERR_INVALID_STATE;
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; ++i) {
        image_hash[i * 2] = hex[digest[i] >> 4];
        image_hash[i * 2 + 1] = hex[digest[i] & 15];
    }
    control = xSemaphoreCreateMutex();
    if (!control)
        return ESP_ERR_NO_MEM;
    health_check = healthy;
    if (xTaskCreate(ota_task, "ota_monitor", 6144, NULL, 1, NULL) != pdPASS) {
        vSemaphoreDelete(control);
        control = NULL;
        return ESP_ERR_NO_MEM;
    }
    started = true;
    return ESP_OK;
}
#else
esp_err_t ota_service_start(bool (*healthy)(void))
{
    (void)healthy;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
