#include "lisp_http_service.h"
#include "sdkconfig.h"
#if !CONFIG_MBEDTLS_CERTIFICATE_BUNDLE || !CONFIG_MBEDTLS_HAVE_TIME || !CONFIG_MBEDTLS_HAVE_TIME_DATE ||     \
    !CONFIG_MBEDTLS_TLS_CLIENT || !CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS
#error "HTTP data API requires CA bundle, certificate dates and HTTPS"
#endif
#if CONFIG_ESP_TLS_INSECURE || CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY
#error "HTTP data API forbids insecure TLS"
#endif
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lisp_clock.h"
#include "wifi_service.h"
#include <stdatomic.h>
#include <strings.h>
static lisp_http_slot_t slot;
static SemaphoreHandle_t gate;
static TaskHandle_t worker;
static atomic_bool started;
/* Accessed only by the worker after accepting one immutable request. */
static esp_http_client_handle_t client;
static uint64_t deadline;
static bool encoded;
static atomic_bool cleanup_failed;
static uint64_t now_ms(void *u)
{
    (void)u;
    return (uint64_t)(esp_timer_get_time() / 1000);
}
static bool cancelled(void *u)
{
    (void)u;
    (void)xSemaphoreTake(gate, portMAX_DELAY);
    bool value = slot.cancelled;
    (void)xSemaphoreGive(gate);
    return value;
}
static esp_err_t headers(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key && event->header_value &&
        !strcasecmp(event->header_key, "Content-Encoding") && strcasecmp(event->header_value, "identity"))
        encoded = true;
    return ESP_OK;
}
static bool open_url(void *u, const char *url, unsigned wait, lisp_http_headers_t *response)
{
    (void)u;
    encoded = false;
    esp_http_client_config_t config = {.url = url,
                                       .method = HTTP_METHOD_GET,
                                       .timeout_ms = (int)wait,
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
                                       .user_agent = "wrover-lisp/0.6"};
    client = esp_http_client_init(&config);
    if (!client || esp_http_client_set_header(client, "Accept-Encoding", "identity") != ESP_OK ||
        esp_http_client_set_header(client, "Accept", "application/json, text/plain") != ESP_OK ||
        esp_http_client_open(client, 0) != ESP_OK)
        return false;
    uint64_t now = now_ms(NULL);
    if (now >= deadline || cancelled(NULL))
        return false;
    uint64_t left = deadline - now;
    int timeout = left < LISP_HTTP_IO_MS ? (int)left : LISP_HTTP_IO_MS;
    if (esp_http_client_set_timeout_ms(client, timeout) != ESP_OK)
        return false;
    int64_t size = esp_http_client_fetch_headers(client);
    if (size < 0)
        return false;
    *response =
        (lisp_http_headers_t){esp_http_client_get_status_code(client), size == 0 ? -1 : size, encoded};
    return true;
}
static int read_body(void *u, char *out, size_t capacity, unsigned wait)
{
    (void)u;
    if (!client || esp_http_client_set_timeout_ms(client, (int)wait) != ESP_OK)
        return -1;
    return esp_http_client_read(client, out, (int)capacity);
}
static bool complete(void *u)
{
    (void)u;
    return client && esp_http_client_is_complete_data_received(client);
}
static void close_url(void *u)
{
    (void)u;
    if (client) {
        cleanup_failed = esp_http_client_cleanup(client) != ESP_OK;
        client = NULL;
    }
}
static lisp_http_error_t request(void *u, const char *url, size_t length, uint32_t *id)
{
    (void)u;
    wifi_link_snapshot_t link;
    if (!wifi_service_get_link_snapshot(&link) || link.phase != WIFI_LINK_ONLINE)
        return LISP_HTTP_OFFLINE;
    (void)xSemaphoreTake(gate, portMAX_DELAY);
    lisp_http_error_t error =
        cleanup_failed ? LISP_HTTP_UNAVAILABLE : lisp_http_slot_request(&slot, url, length, id);
    if (error == LISP_HTTP_OK)
        deadline = now_ms(NULL) + LISP_HTTP_TIMEOUT_MS;
    (void)xSemaphoreGive(gate);
    if (error == LISP_HTTP_OK)
        xTaskNotifyGive(worker);
    return error;
}
static lisp_http_phase_t poll_result(void *u, uint32_t id, lisp_http_result_t *result)
{
    (void)u;
    (void)xSemaphoreTake(gate, portMAX_DELAY);
    lisp_http_phase_t phase = lisp_http_slot_poll(&slot, id, result);
    (void)xSemaphoreGive(gate);
    return phase;
}
static bool cancel(void *u, uint32_t id)
{
    (void)u;
    (void)xSemaphoreTake(gate, portMAX_DELAY);
    bool ok = lisp_http_slot_cancel(&slot, id);
    (void)xSemaphoreGive(gate);
    return ok;
}
static bool release(void *u, uint32_t id)
{
    (void)u;
    (void)xSemaphoreTake(gate, portMAX_DELAY);
    bool ok = lisp_http_slot_release(&slot, id);
    (void)xSemaphoreGive(gate);
    return ok;
}
static void worker_task(void *arg)
{
    (void)arg;
    const lisp_http_io_t io = {NULL, now_ms, cancelled, open_url, read_body, complete, close_url};
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        lisp_http_result_t result = {.error = LISP_HTTP_CLOCK};
        if (lisp_clock_service_ensure(deadline, cancelled, NULL))
            result = lisp_http_download(&io, slot.url, slot.buffer, slot.capacity, deadline);
        else if (cancelled(NULL))
            result.error = LISP_HTTP_CANCELLED;
        else if (now_ms(NULL) >= deadline)
            result.error = LISP_HTTP_TIMEOUT;
        (void)xSemaphoreTake(gate, portMAX_DELAY);
        if (cleanup_failed)
            result = (lisp_http_result_t){.error = LISP_HTTP_UNAVAILABLE};
        lisp_http_slot_finish(&slot, result);
        (void)xSemaphoreGive(gate);
    }
}
bool lisp_http_service_start(lisp_http_backend_t *backend)
{
    bool expected = false;
    if (!backend || !atomic_compare_exchange_strong(&started, &expected, true))
        return false;
    char *buffer = heap_caps_calloc(1, LISP_HTTP_BODY_MAX + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    gate = xSemaphoreCreateMutex();
    if (!buffer || !gate || !lisp_http_slot_init(&slot, buffer, LISP_HTTP_BODY_MAX + 1)) {
        if (gate)
            vSemaphoreDelete(gate);
        heap_caps_free(buffer);
        return false;
    }
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_WARN);
    if (xTaskCreate(worker_task, "lisp_http", 8192, NULL, 1, &worker) != pdPASS) {
        vSemaphoreDelete(gate);
        heap_caps_free(buffer);
        return false;
    }
    *backend = (lisp_http_backend_t){NULL, request, poll_result, cancel, release};
    return true;
}
