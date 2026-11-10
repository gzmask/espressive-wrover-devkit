#include "lisp_app_url_service.h"
#include "sdkconfig.h"
#if !CONFIG_MBEDTLS_CERTIFICATE_BUNDLE || !CONFIG_MBEDTLS_HAVE_TIME || !CONFIG_MBEDTLS_HAVE_TIME_DATE ||     \
    !CONFIG_MBEDTLS_TLS_CLIENT || !CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS
#error "URL loader requires CA bundle, certificate validity dates and HTTPS"
#endif
#if CONFIG_ESP_TLS_INSECURE || CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY
#error "URL loader forbids insecure TLS"
#endif
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "wifi_service.h"
#include <stdatomic.h>
#include <string.h>
#include <strings.h>
#include <time.h>

static lisp_app_url_slot_t slot;
static SemaphoreHandle_t gate;
static TaskHandle_t worker;
static atomic_bool started;
/* Worker-only transport state. No stack object or Lisp value crosses tasks. */
static esp_http_client_handle_t client;
static bool encoded;
static uint64_t deadline;
static uint64_t now_ms(void *user)
{
    (void)user;
    return (uint64_t)(esp_timer_get_time() / 1000);
}
static bool cancelled(void *user)
{
    (void)user;
    (void)xSemaphoreTake(gate, portMAX_DELAY);
    bool result = slot.cancelled;
    (void)xSemaphoreGive(gate);
    return result;
}
static unsigned remaining(void)
{
    uint64_t now = now_ms(NULL);
    if (now >= deadline || cancelled(NULL))
        return 0;
    uint64_t left = deadline - now;
    return left < LISP_APP_URL_IO_TIMEOUT_MS ? (unsigned)left : LISP_APP_URL_IO_TIMEOUT_MS;
}
static bool clock_ready(void)
{
    /* UTC validity is checked by mbedTLS. SNTP is request-triggered, not a
     * persistent background service, and is NOT an authenticated time source. */
    if (time(NULL) >= (time_t)1735689600) /* 2025-01-01 */
        return true;
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    if (esp_netif_sntp_init(&config) != ESP_OK)
        return false;
    bool synced = false;
    uint64_t until = now_ms(NULL) + 10000;
    while (remaining() && now_ms(NULL) < until) {
        if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(100)) == ESP_OK) {
            synced = true;
            break;
        }
    }
    esp_netif_sntp_deinit(); /* Always stop: no ongoing periodic synchronization. */
    return synced && time(NULL) >= (time_t)1735689600;
}
static esp_err_t headers(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key && event->header_value &&
        !strcasecmp(event->header_key, "Content-Encoding") && strcasecmp(event->header_value, "identity"))
        encoded = true;
    return ESP_OK;
}
static bool open_url(void *user, const char *url, unsigned timeout_ms, lisp_app_url_response_t *response)
{
    (void)user;
    encoded = false;
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = (int)timeout_ms,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .auth_type = HTTP_AUTH_TYPE_NONE,
        .max_authorization_retries = -1,
        .disable_auto_redirect = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = false,
        .event_handler = headers,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
        .keep_alive_enable = false,
        .addr_type = HTTP_ADDR_TYPE_INET,
    };
    client = esp_http_client_init(&config);
    if (!client || esp_http_client_set_header(client, "Accept-Encoding", "identity") != ESP_OK ||
        esp_http_client_set_header(client, "Accept", "text/plain, application/octet-stream") != ESP_OK ||
        esp_http_client_open(client, 0) != ESP_OK)
        return false;
    unsigned wait = remaining();
    if (!wait || esp_http_client_set_timeout_ms(client, (int)wait) != ESP_OK)
        return false;
    int64_t size = esp_http_client_fetch_headers(client);
    if (size < 0)
        return false;
    *response =
        (lisp_app_url_response_t){esp_http_client_get_status_code(client), size == 0 ? -1 : size, encoded};
    return remaining() != 0;
}
static int read_url(void *user, char *buffer, size_t capacity, unsigned timeout_ms)
{
    (void)user;
    if (!client || esp_http_client_set_timeout_ms(client, (int)timeout_ms) != ESP_OK)
        return -1;
    return esp_http_client_read(client, buffer, (int)capacity);
}
static bool complete_url(void *user)
{
    (void)user;
    return client && esp_http_client_is_complete_data_received(client);
}
static void close_url(void *user)
{
    (void)user;
    if (client) {
        (void)esp_http_client_cleanup(client);
        client = NULL;
    }
}
static bool request(void *user, const char *url, size_t length)
{
    (void)user;
    wifi_link_snapshot_t link;
    if (!wifi_service_get_link_snapshot(&link) || link.phase != WIFI_LINK_ONLINE)
        return false;
    (void)xSemaphoreTake(gate, portMAX_DELAY);
    bool accepted = lisp_app_url_slot_request(&slot, url, length);
    if (accepted)
        deadline = now_ms(NULL) + LISP_APP_URL_TIMEOUT_MS;
    (void)xSemaphoreGive(gate);
    if (accepted)
        xTaskNotifyGive(worker);
    return accepted;
}
static lisp_app_url_phase_t poll_result(void *user, const char **source, size_t *length)
{
    (void)user;
    (void)xSemaphoreTake(gate, portMAX_DELAY);
    lisp_app_url_phase_t result = lisp_app_url_slot_poll(&slot, source, length);
    (void)xSemaphoreGive(gate);
    return result;
}
static void cancel(void *user)
{
    (void)user;
    (void)xSemaphoreTake(gate, portMAX_DELAY);
    lisp_app_url_slot_cancel(&slot);
    (void)xSemaphoreGive(gate);
}
static void release(void *user)
{
    (void)user;
    (void)xSemaphoreTake(gate, portMAX_DELAY);
    lisp_app_url_slot_release(&slot);
    (void)xSemaphoreGive(gate);
}
static void worker_task(void *argument)
{
    (void)argument;
    const lisp_app_url_io_t io = {NULL, now_ms, cancelled, open_url, read_url, complete_url, close_url};
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        size_t length = 0;
        /* Request URL/buffer cannot change until this worker finishes, even if
         * owner cancels or resets. Metadata mutex is NEVER held during IO. */
        bool success = remaining() && clock_ready() &&
                       lisp_app_url_download(&io, slot.url, slot.source, slot.capacity, &length, deadline);
        (void)xSemaphoreTake(gate, portMAX_DELAY);
        lisp_app_url_slot_finish(&slot, success, length);
        (void)xSemaphoreGive(gate);
    }
}
bool lisp_app_url_service_start(lisp_app_url_backend_t *backend)
{
    bool expected = false;
    if (!backend || !atomic_compare_exchange_strong(&started, &expected, true))
        return false;
    char *buffer = heap_caps_calloc(1, LISP_APP_URL_SOURCE_CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    gate = xSemaphoreCreateMutex();
    if (!buffer || !gate || !lisp_app_url_slot_init(&slot, buffer, LISP_APP_URL_SOURCE_CAPACITY)) {
        if (gate)
            vSemaphoreDelete(gate);
        heap_caps_free(buffer);
        return false;
    }
    /* Prevent SDK debug/info output from logging URL query strings or source.
     * No custom global log callback or network configuration is installed. */
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_WARN);
    if (xTaskCreate(worker_task, "app_url", 8192, NULL, 1, &worker) != pdPASS) {
        vSemaphoreDelete(gate);
        heap_caps_free(buffer);
        return false;
    }
    *backend = (lisp_app_url_backend_t){NULL, request, poll_result, cancel, release};
    return true;
}
