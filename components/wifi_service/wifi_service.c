#include "wifi_service.h"
#include "sdkconfig.h"
#include "wifi_station.h"

bool wifi_service_get_link_snapshot(wifi_link_snapshot_t *out)
{
    return node_wifi_station_get_snapshot(out);
}

#if CONFIG_WIFI_SERVICE_ENABLE && !CONFIG_WIFI_SERVICE_CONNECT_ENABLE
#if CONFIG_ESP_WIFI_NVS_ENABLED || CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE ||                            \
    CONFIG_ESP_PHY_INIT_DATA_IN_PARTITION
#error "RAM-only scan requires Wi-Fi NVS, PHY calibration storage and PHY data partition disabled"
#endif

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <string.h>

enum { WIFI_STACK_BYTES = 6144, SCAN_TIMEOUT_MS = 12000 };
static const char *TAG = "wifi_service";
static atomic_bool started;
static portMUX_TYPE snapshot_lock = portMUX_INITIALIZER_UNLOCKED;
static wifi_scan_snapshot_t snapshot;
/* Static internal RAM; never freed while the event loop may hold its address.
 * The owner task stays alive even on a fatal cleanup error. */
static struct {
    TaskHandle_t task;
    esp_event_handler_instance_t handler;
    bool initialized;
    bool running;
    bool registered;
    wifi_ap_record_t records[WIFI_SCAN_MAX_RECORDS];
} context;

bool wifi_service_get_snapshot(wifi_scan_snapshot_t *out)
{
    if (out == NULL)
        return false;
    portENTER_CRITICAL(&snapshot_lock);
    *out = snapshot;
    portEXIT_CRITICAL(&snapshot_lock);
    return true;
}

static void publish(const wifi_scan_snapshot_t *status)
{
    portENTER_CRITICAL(&snapshot_lock);
    snapshot = *status;
    portEXIT_CRITICAL(&snapshot_lock);
    char json[256];
    if (wifi_scan_format_status(status, json, sizeof(json)))
        ESP_LOGI(TAG, "wifi_status=%s", json);
    else
        ESP_LOGE(TAG, "Wi-Fi status serialization failed");
}

static void scan_done(void *argument, esp_event_base_t base, int32_t id, void *data)
{
    (void)argument;
    if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        const wifi_event_sta_scan_done_t *event = data;
        (void)xTaskNotify(context.task, event != NULL ? event->status : UINT32_MAX, eSetValueWithOverwrite);
    }
}

static esp_err_t initialize_radio(void)
{
    esp_err_t error = esp_netif_init();
    if (error != ESP_OK) {
        return error;
    }
    error = esp_event_loop_create_default();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
        return error;
    /* Global network support only: no IP interface or DHCP client is created. */
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    config.nvs_enable = 0;
    error = esp_wifi_init(&config);
    if (error != ESP_OK)
        return error;
    context.initialized = true;
    error = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (error != ESP_OK)
        return error;
    error = esp_wifi_set_mode(WIFI_MODE_STA);
    if (error != ESP_OK)
        return error;
    wifi_country_t country = {0};
    error = esp_wifi_get_country(&country);
    if (error != ESP_OK)
        return error;
    /* Do not guess the user's country or call setters documented to persist.
     * Fail closed if the pinned driver's world-safe defaults are unexpected. */
    if (memcmp(country.cc, "01", 2) != 0 || country.schan != 1 || country.nchan != 11)
        return ESP_ERR_NOT_SUPPORTED;
    error = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, scan_done, NULL,
                                                &context.handler);
    if (error != ESP_OK)
        return error;
    context.registered = true;
    error = esp_wifi_start();
    if (error == ESP_OK)
        context.running = true;
    return error;
}

static wifi_scan_security_t security_type(wifi_auth_mode_t auth)
{
    switch (auth) {
    case WIFI_AUTH_OPEN:
        return WIFI_SCAN_AUTH_OPEN;
    case WIFI_AUTH_WEP:
        return WIFI_SCAN_AUTH_WEP;
    case WIFI_AUTH_WPA_PSK:
        return WIFI_SCAN_AUTH_WPA;
    case WIFI_AUTH_WPA2_PSK:
        return WIFI_SCAN_AUTH_WPA2;
    case WIFI_AUTH_WPA_WPA2_PSK:
        return WIFI_SCAN_AUTH_WPA_WPA2;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return WIFI_SCAN_AUTH_WPA2_ENTERPRISE;
    case WIFI_AUTH_WPA3_PSK:
        return WIFI_SCAN_AUTH_WPA3;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return WIFI_SCAN_AUTH_WPA2_WPA3;
    case WIFI_AUTH_OWE:
        return WIFI_SCAN_AUTH_OWE;
    default:
        return WIFI_SCAN_AUTH_UNKNOWN;
    }
}

static esp_err_t read_scan_results(uint16_t *total, uint16_t *reported)
{
    esp_err_t error = esp_wifi_scan_get_ap_num(total);
    if (error != ESP_OK)
        return error;
    if (*total == 0) {
        *reported = 0;
        return esp_wifi_clear_ap_list();
    }
    *reported = WIFI_SCAN_MAX_RECORDS;
    memset(context.records, 0, sizeof(context.records));
    /* This API frees the ENTIRE driver list even when only 16 records fit. */
    error = esp_wifi_scan_get_ap_records(reported, context.records);
    if (error != ESP_OK)
        return error;
    if (*reported > WIFI_SCAN_MAX_RECORDS || *reported > *total)
        return ESP_ERR_INVALID_SIZE;
    for (uint16_t i = 0; i < *reported; i++) {
        const wifi_ap_record_t *record = &context.records[i];
        wifi_scan_ap_t ap = {.channel = record->primary,
                             .rssi = record->rssi,
                             .security = security_type(record->authmode),
                             .authmode_id = (int32_t)record->authmode};
        memcpy(ap.ssid, record->ssid, sizeof(ap.ssid));
        memcpy(ap.bssid, record->bssid, sizeof(ap.bssid));
        while (ap.ssid_length < sizeof(ap.ssid) && ap.ssid[ap.ssid_length] != 0)
            ap.ssid_length++;
        char json[512];
        if (!wifi_scan_format_ap(&ap, json, sizeof(json)))
            return ESP_ERR_INVALID_RESPONSE;
        ESP_LOGI(TAG, "scan_ap=%s", json);
    }
    return ESP_OK;
}

static void cleanup_error(const char *operation, esp_err_t error)
{
    if (error != ESP_OK)
        ESP_LOGE(TAG, "%s failed during cleanup: %s", operation, esp_err_to_name(error));
}

static void stop_radio(void)
{
    bool stopped = true;
    if (context.running) {
        cleanup_error("scan_stop", esp_wifi_scan_stop());
        cleanup_error("clear_ap_list", esp_wifi_clear_ap_list());
        esp_err_t error = esp_wifi_stop();
        stopped = error == ESP_OK;
        cleanup_error("wifi_stop", error);
    }
    if (context.registered)
        cleanup_error("handler_unregister", esp_event_handler_instance_unregister(
                                                WIFI_EVENT, WIFI_EVENT_SCAN_DONE, context.handler));
    if (context.initialized && stopped)
        cleanup_error("wifi_deinit", esp_wifi_deinit());
    /* Keep the shared default loop, static callback context and owner task.
     * Even an unregister/stop failure cannot notify a deleted task. */
}

static void scan_task(void *argument)
{
    (void)argument;
    context.task = xTaskGetCurrentTaskHandle();
    wifi_scan_snapshot_t state = wifi_scan_initial();
    (void)wifi_scan_starting(&state);
    /* Let LCD identification/first frame and heartbeat start before RF init. */
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_log_level_set("wifi", ESP_LOG_WARN); /* SDK info logs include the device MAC. */
    ESP_LOGI(TAG, "RAM-only passive scan; world-safe 2.4GHz channels 1-11; no association/IP/HTTP");
    ESP_LOGW(TAG, "SSID hex is reversible; scan logs are private. Full PHY calibration each boot.");
    esp_err_t error = initialize_radio();
    while (error == ESP_OK) {
        if (!wifi_scan_begin(&state)) {
            error = ESP_ERR_INVALID_STATE;
            break;
        }
        publish(&state);
        /* Explicit passive scan: NULL would select ACTIVE scanning. No setters
         * for country, credentials, raw transmission or connection are used. */
        wifi_scan_config_t config = {
            .channel = 0, .show_hidden = true, .scan_type = WIFI_SCAN_TYPE_PASSIVE, .scan_time.passive = 360};
        uint32_t result = UINT32_MAX;
        error = esp_wifi_scan_start(&config, false);
        if (error != ESP_OK)
            break;
        if (xTaskNotifyWait(0, UINT32_MAX, &result, pdMS_TO_TICKS(SCAN_TIMEOUT_MS)) != pdTRUE) {
            error = ESP_ERR_TIMEOUT;
            break;
        }
        if (result != 0) {
            error = ESP_FAIL;
            break;
        }
        uint16_t total = 0, reported = 0;
        error = read_scan_results(&total, &reported);
        if (error != ESP_OK)
            break;
        if (!wifi_scan_finish(&state, total, reported)) {
            error = ESP_ERR_INVALID_STATE;
            break;
        }
        publish(&state);
        ESP_LOGI(TAG, "scan complete; task stack low-water=%u bytes",
                 (unsigned)uxTaskGetStackHighWaterMark(NULL));
        vTaskDelay(pdMS_TO_TICKS(CONFIG_WIFI_SERVICE_SCAN_INTERVAL_SECONDS * 1000));
    }
    (void)wifi_scan_fail(&state, (int32_t)error);
    publish(&state);
    ESP_LOGE(TAG, "scan service stopped: %s; LCD and heartbeat remain independent", esp_err_to_name(error));
    stop_radio();
    /* Terminal until reset: no stale completion can trigger a new scan. */
    for (;;)
        vTaskDelay(pdMS_TO_TICKS(60000));
}

esp_err_t wifi_service_start(void)
{
    bool expected = false;
    if (!atomic_compare_exchange_strong(&started, &expected, true))
        return ESP_ERR_INVALID_STATE;
    wifi_scan_snapshot_t state = wifi_scan_initial();
    (void)wifi_scan_starting(&state);
    publish(&state);
    if (xTaskCreate(scan_task, "wifi_scan", WIFI_STACK_BYTES, NULL, 1, NULL) != pdPASS) {
        (void)wifi_scan_fail(&state, ESP_ERR_NO_MEM);
        publish(&state);
        atomic_store(&started, false);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

#else
bool wifi_service_get_snapshot(wifi_scan_snapshot_t *out)
{
    if (out == NULL)
        return false;
    *out = wifi_scan_initial();
    return true;
}
esp_err_t wifi_service_start(void)
{
#if CONFIG_WIFI_SERVICE_ENABLE && CONFIG_WIFI_SERVICE_CONNECT_ENABLE
    return node_wifi_station_start();
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
#endif
