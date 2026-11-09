#include "wifi_station.h"
#include "sdkconfig.h"

#if CONFIG_WIFI_SERVICE_ENABLE && CONFIG_WIFI_SERVICE_CONNECT_ENABLE
#if !CONFIG_WIFI_SERVICE_US_CONFIRMED
#error "Confirm actual US location before enabling station mode"
#endif
#if CONFIG_ESP_WIFI_NVS_ENABLED || CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE ||                            \
    CONFIG_ESP_PHY_INIT_DATA_IN_PARTITION || CONFIG_ESP_PHY_MULTIPLE_INIT_DATA_BIN ||                        \
    CONFIG_LWIP_DHCP_RESTORE_LAST_IP
#error "Station mode requires Wi-Fi/PHY/DHCP persistence and PHY partition options disabled"
#endif
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "wifi_provision.h"
#include <stdatomic.h>
#include <string.h>

enum { LINK_STACK_BYTES = 8192, EVENT_COUNT = 16, STOP_TIMEOUT_MS = 5000 };
typedef enum {
    LINK_START,
    LINK_STOP,
    LINK_ASSOCIATED,
    LINK_DISCONNECTED,
    LINK_GOT_IP,
    LINK_LOST_IP
} event_kind_t;
typedef struct {
    event_kind_t kind;
    uint16_t reason;
    uint8_t channel;
    uint8_t ipv4[4];
} link_event_t;
static const char *TAG = "wifi_station";
static atomic_bool started;
static atomic_bool event_overflow;
static portMUX_TYPE snapshot_lock = portMUX_INITIALIZER_UNLOCKED;
static wifi_link_snapshot_t snapshot;
/* Static internal RAM; callbacks never reference a deleted task/queue/netif. */
static struct {
    QueueHandle_t events;
    StaticQueue_t queue_control;
    uint8_t queue_storage[EVENT_COUNT * sizeof(link_event_t)];
    esp_netif_t *netif;
    esp_event_handler_instance_t wifi_handler;
    esp_event_handler_instance_t ip_handler;
    bool radio_running;
    bool start_pending;
    bool stop_pending;
    uint64_t stop_deadline;
    uint8_t ssid[32];
    size_t ssid_length;
} context;

static uint64_t now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}
bool node_wifi_station_get_snapshot(wifi_link_snapshot_t *out)
{
    if (out == NULL)
        return false;
    portENTER_CRITICAL(&snapshot_lock);
    *out = snapshot;
    portEXIT_CRITICAL(&snapshot_lock);
    return true;
}
static void publish(const wifi_link_snapshot_t *state)
{
    portENTER_CRITICAL(&snapshot_lock);
    snapshot = *state;
    portEXIT_CRITICAL(&snapshot_lock);
    char json[512];
    if (wifi_link_format_json(state, now_ms(), json, sizeof(json)))
        ESP_LOGI(TAG, "wifi_status=%s", json);
    else
        ESP_LOGE(TAG, "station status serialization failed");
}
static void on_event(void *argument, esp_event_base_t base, int32_t id, void *data)
{
    (void)argument;
    link_event_t event = {0};
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            event.kind = LINK_START;
            break;
        case WIFI_EVENT_STA_STOP:
            event.kind = LINK_STOP;
            break;
        case WIFI_EVENT_STA_CONNECTED:
            if (data == NULL)
                return;
            event.kind = LINK_ASSOCIATED;
            event.channel = ((const wifi_event_sta_connected_t *)data)->channel;
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            if (data == NULL)
                return;
            event.kind = LINK_DISCONNECTED;
            event.reason = ((const wifi_event_sta_disconnected_t *)data)->reason;
            break;
        default:
            return;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *ip = data;
        if (ip == NULL || ip->esp_netif != context.netif)
            return;
        event.kind = LINK_GOT_IP;
        /* ESP IPv4 address stores octets in network order in memory. */
        memcpy(event.ipv4, &ip->ip_info.ip.addr, sizeof(event.ipv4));
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        event.kind = LINK_LOST_IP;
    } else
        return;
    if (xQueueSend(context.events, &event, 0) != pdTRUE)
        atomic_store(&event_overflow, true); /* Owner checks at least every 250 ms. */
}
/* Linked directly from the private local two-line file; never logged. */
extern const uint8_t _binary_node_wifi_credentials_start[];
extern const uint32_t node_wifi_credentials_length;

static esp_err_t initialize_radio(const wifi_credentials_t *credentials)
{
    esp_err_t error = esp_netif_init();
    if (error != ESP_OK)
        return error;
    error = esp_event_loop_create_default();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
        return error;
    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_WIFI_STA();
    context.netif = esp_netif_new(&netif_config);
    if (context.netif == NULL)
        return ESP_ERR_NO_MEM;
    error = esp_netif_attach_wifi_station(context.netif);
    if (error != ESP_OK)
        return error;
    error = esp_wifi_set_default_wifi_sta_handlers();
    if (error != ESP_OK)
        return error;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    init.nvs_enable = 0;
    error = esp_wifi_init(&init);
    if (error != ESP_OK)
        return error;
    error = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (error != ESP_OK)
        return error;
    error = esp_wifi_set_mode(WIFI_MODE_STA);
    if (error != ESP_OK)
        return error;
    /* Pinned IDF country test explicitly uses nvs_enable=false with this API.
     * Read-back is mandatory; no AP-learned regulatory-domain changes. */
    error = esp_wifi_set_country_code("US", false);
    if (error != ESP_OK)
        return error;
    wifi_country_t country = {0};
    error = esp_wifi_get_country(&country);
    if (error != ESP_OK)
        return error;
    if (memcmp(country.cc, "US", 2) != 0 || country.schan != 1 || country.nchan != 11 ||
        country.policy != WIFI_COUNTRY_POLICY_MANUAL)
        return ESP_ERR_NOT_SUPPORTED;
    ESP_LOGI(TAG, "country_verified=US channels=1-11 policy=manual; NVS/PHY persistence disabled");
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, credentials->ssid, credentials->ssid_length);
    memcpy(config.sta.password, credentials->password, credentials->password_length);
    config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;
    config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    config.sta.failure_retry_cnt = 0; /* Only the owner schedules bounded retries. */
    error = esp_wifi_set_config(WIFI_IF_STA, &config);
    wifi_provision_wipe(&config, sizeof(config));
    if (error != ESP_OK)
        return error;
    memcpy(context.ssid, credentials->ssid, credentials->ssid_length);
    context.ssid_length = credentials->ssid_length;
    error = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL,
                                                &context.wifi_handler);
    if (error != ESP_OK)
        return error;
    return esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, on_event, NULL,
                                               &context.ip_handler);
}
static esp_err_t target_ap(wifi_ap_record_t *record)
{
    esp_err_t error = esp_wifi_sta_get_ap_info(record);
    if (error != ESP_OK)
        return error;
    if (strnlen((const char *)record->ssid, sizeof(record->ssid)) != context.ssid_length ||
        memcmp(record->ssid, context.ssid, context.ssid_length) != 0 || record->primary < 1 ||
        record->primary > 11 ||
        (record->authmode != WIFI_AUTH_WPA2_PSK && record->authmode != WIFI_AUTH_WPA3_PSK &&
         record->authmode != WIFI_AUTH_WPA2_WPA3_PSK))
        return ESP_ERR_NOT_SUPPORTED;
    return ESP_OK;
}
static esp_err_t begin_attempt(wifi_link_snapshot_t *state)
{
    if (context.radio_running || context.stop_pending)
        return ESP_ERR_INVALID_STATE;
    esp_netif_ip_info_t ip = {0};
    esp_err_t error = esp_netif_get_ip_info(context.netif, &ip);
    if (error != ESP_OK)
        return error;
    if (ip.ip.addr != 0)
        return ESP_ERR_INVALID_STATE; /* Never inherit a stale lease. */
    if (!wifi_link_begin(state, now_ms()))
        return ESP_ERR_INVALID_STATE;
    context.start_pending = true;
    publish(state);
    error = esp_wifi_start();
    if (error == ESP_OK)
        context.radio_running = true;
    return error;
}
static esp_err_t schedule_failure(wifi_link_snapshot_t *state, uint16_t reason, esp_err_t error)
{
    if (!wifi_link_failed(state, reason, error, now_ms()))
        return ESP_ERR_INVALID_STATE;
    publish(state); /* Clear association/IP immediately, before driver cleanup. */
    context.start_pending = false;
    if (context.radio_running) {
        context.stop_pending = true;
        context.stop_deadline = now_ms() + STOP_TIMEOUT_MS;
        error = esp_wifi_stop();
        if (error != ESP_OK)
            return error;
        context.radio_running = false;
    }
    return ESP_OK;
}
static esp_err_t handle_event(wifi_link_snapshot_t *state, const link_event_t *event)
{
    switch (event->kind) {
    case LINK_START:
        if (state->phase == WIFI_LINK_CONNECTING && context.start_pending) {
            context.start_pending = false;
            return esp_wifi_connect();
        }
        return ESP_OK;
    case LINK_STOP:
        if (!context.stop_pending && state->phase != WIFI_LINK_BACKOFF && state->phase != WIFI_LINK_ERROR)
            return ESP_ERR_INVALID_STATE;
        context.stop_pending = false;
        return ESP_OK;
    case LINK_ASSOCIATED:
        if (state->phase == WIFI_LINK_CONNECTING && !context.start_pending) {
            wifi_ap_record_t ap = {0};
            esp_err_t error = target_ap(&ap);
            if (error == ESP_ERR_WIFI_NOT_CONNECT)
                return ESP_OK; /* Default handlers may already have processed a queued disconnect. */
            if (error != ESP_OK)
                return error;
            if (ap.primary != event->channel || !wifi_link_associated(state, event->channel, now_ms()))
                return ESP_ERR_INVALID_STATE;
            publish(state);
        }
        return ESP_OK;
    case LINK_DISCONNECTED:
        if (state->phase == WIFI_LINK_CONNECTING || state->phase == WIFI_LINK_DHCP ||
            state->phase == WIFI_LINK_ONLINE)
            return schedule_failure(state, event->reason, ESP_OK);
        return ESP_OK;
    case LINK_GOT_IP:
        if (state->phase == WIFI_LINK_DHCP || state->phase == WIFI_LINK_ONLINE) {
            esp_netif_ip_info_t current = {0};
            wifi_ap_record_t ap = {0};
            esp_err_t error = target_ap(&ap);
            if (error == ESP_ERR_WIFI_NOT_CONNECT)
                return ESP_OK;
            if (error != ESP_OK)
                return error;
            error = esp_netif_get_ip_info(context.netif, &current);
            if (error != ESP_OK)
                return error;
            if (!esp_netif_is_netif_up(context.netif) || memcmp(&current.ip.addr, event->ipv4, 4) != 0)
                return ESP_OK; /* Late/stale event; current SDK state is authoritative. */
            if (state->phase == WIFI_LINK_ONLINE) {
                /* DHCP renewal is accepted only after rechecking the current lease. */
                if (!wifi_link_renew_ipv4(state, event->ipv4))
                    return ESP_ERR_INVALID_ARG;
            } else if (!wifi_link_ipv4(state, event->ipv4, now_ms()))
                return ESP_ERR_INVALID_ARG;
            state->channel = ap.primary;
            state->rssi_valid = false;
            (void)wifi_link_signal(state, ap.rssi);
            publish(state);
        }
        return ESP_OK;
    case LINK_LOST_IP:
        if (state->phase == WIFI_LINK_ONLINE) {
            esp_netif_ip_info_t ip = {0};
            esp_err_t error = esp_netif_get_ip_info(context.netif, &ip);
            if (error != ESP_OK)
                return error;
            if (ip.ip.addr == 0)
                return schedule_failure(state, 0, ESP_ERR_INVALID_STATE);
        }
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}
static void station_task(void *argument)
{
    (void)argument;
    wifi_link_snapshot_t state = wifi_link_initial();
    (void)wifi_link_configure(&state, now_ms());
    publish(&state);
    vTaskDelay(pdMS_TO_TICKS(3000)); /* LCD/heartbeat first. */
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("wpa", ESP_LOG_WARN);
    esp_log_level_set("wpa_supplicant", ESP_LOG_WARN);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_WARN);
    wifi_credentials_t credentials = {0};
    esp_err_t error = wifi_credentials_from_text(_binary_node_wifi_credentials_start,
                                                 node_wifi_credentials_length, &credentials)
                          ? ESP_OK
                          : ESP_ERR_INVALID_ARG;
    if (error == ESP_OK)
        error = initialize_radio(&credentials);
    wifi_provision_wipe(&credentials, sizeof(credentials));
    if (error == ESP_OK) {
        ESP_LOGI(TAG, "embedded_credentials_applied; automatic connection, no HTTP");
        error = begin_attempt(&state);
    }
    uint64_t last_report = now_ms();
    while (error == ESP_OK && state.phase != WIFI_LINK_ERROR) {
        if (atomic_load(&event_overflow)) {
            error = ESP_ERR_NO_MEM;
            break;
        }
        link_event_t event;
        if (xQueueReceive(context.events, &event, pdMS_TO_TICKS(250)) == pdTRUE)
            error = handle_event(&state, &event);
        if (error != ESP_OK)
            break;
        uint64_t now = now_ms();
        if (context.stop_pending && now >= context.stop_deadline) {
            error = ESP_ERR_TIMEOUT;
            break;
        }
        if (wifi_link_due(&state, now)) {
            if (state.phase == WIFI_LINK_BACKOFF) {
                if (!context.stop_pending)
                    error = begin_attempt(&state);
            } else {
                error = schedule_failure(&state, 0, ESP_ERR_TIMEOUT);
            }
        }
        (void)wifi_link_stable(&state, now);
        if (now - last_report >= 5000) {
            if (state.phase == WIFI_LINK_ONLINE) {
                wifi_ap_record_t ap = {0};
                esp_err_t info = target_ap(&ap);
                if (info == ESP_ERR_NOT_SUPPORTED)
                    error = info; /* Security/country/target mismatch is terminal, not a retry. */
                else if (info != ESP_OK)
                    error = schedule_failure(&state, 0, info);
                else {
                    esp_netif_ip_info_t lease = {0};
                    esp_err_t lease_error = esp_netif_get_ip_info(context.netif, &lease);
                    if (lease_error != ESP_OK || !esp_netif_is_netif_up(context.netif) || lease.ip.addr == 0)
                        error = schedule_failure(&state, 0,
                                                 lease_error == ESP_OK ? ESP_ERR_INVALID_STATE : lease_error);
                    else if (!wifi_link_renew_ipv4(&state, (const uint8_t *)&lease.ip.addr))
                        error = ESP_ERR_INVALID_ARG;
                    else {
                        state.channel = ap.primary;
                        state.rssi_valid = false;
                        (void)wifi_link_signal(&state, ap.rssi);
                    }
                }
            }
            publish(&state);
            ESP_LOGI(TAG, "station task stack low-water=%u bytes",
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
            last_report = now;
        }
    }
    if (state.phase != WIFI_LINK_ERROR)
        (void)wifi_link_fatal(&state, error == ESP_OK ? ESP_FAIL : error, now_ms());
    publish(&state);
    ESP_LOGE(TAG, "station terminal error; no further attempts until reset; LCD/heartbeat independent");
    if (context.radio_running) {
        esp_err_t stopped = esp_wifi_stop();
        if (stopped != ESP_OK)
            ESP_LOGE(TAG, "terminal radio stop failed; reset required");
    }
    /* Never destroy the shared loop/static queue/netif or delete this owner.
     * Late default SDK and user callbacks may still hold their addresses. */
    for (;;)
        vTaskDelay(pdMS_TO_TICKS(60000));
}
esp_err_t node_wifi_station_start(void)
{
    bool expected = false;
    if (!atomic_compare_exchange_strong(&started, &expected, true))
        return ESP_ERR_INVALID_STATE;
    context.events =
        xQueueCreateStatic(EVENT_COUNT, sizeof(link_event_t), context.queue_storage, &context.queue_control);
    if (context.events == NULL ||
        xTaskCreate(station_task, "wifi_station", LINK_STACK_BYTES, NULL, 1, NULL) != pdPASS) {
        wifi_link_snapshot_t state = wifi_link_initial();
        (void)wifi_link_fatal(&state, ESP_ERR_NO_MEM, now_ms());
        publish(&state);
        atomic_store(&started, false);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
#else
bool node_wifi_station_get_snapshot(wifi_link_snapshot_t *out)
{
    if (out == NULL)
        return false;
    *out = wifi_link_initial();
    return true;
}
esp_err_t node_wifi_station_start(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
