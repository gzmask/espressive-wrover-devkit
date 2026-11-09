#include "wifi_scan_logic.h"

#include <limits.h>
#include <stdio.h>

#include "cJSON.h"

wifi_scan_snapshot_t wifi_scan_initial(void)
{
    return (wifi_scan_snapshot_t){.phase = WIFI_SCAN_OFF};
}

const char *wifi_scan_phase_name(wifi_scan_phase_t phase)
{
    switch (phase) {
    case WIFI_SCAN_OFF:
        return "off";
    case WIFI_SCAN_STARTING:
        return "starting";
    case WIFI_SCAN_SCANNING:
        return "scanning";
    case WIFI_SCAN_IDLE:
        return "idle";
    case WIFI_SCAN_ERROR:
        return "error";
    default:
        return NULL;
    }
}

static bool valid_status(const wifi_scan_snapshot_t *status)
{
    return status != NULL && wifi_scan_phase_name(status->phase) != NULL &&
           status->reported_count <= status->ap_count && status->reported_count <= WIFI_SCAN_MAX_RECORDS &&
           ((status->phase == WIFI_SCAN_ERROR) == (status->last_error != 0));
}

bool wifi_scan_starting(wifi_scan_snapshot_t *status)
{
    if (!valid_status(status) || status->phase != WIFI_SCAN_OFF)
        return false;
    status->phase = WIFI_SCAN_STARTING;
    return true;
}

bool wifi_scan_begin(wifi_scan_snapshot_t *status)
{
    if (!valid_status(status) || (status->phase != WIFI_SCAN_STARTING && status->phase != WIFI_SCAN_IDLE))
        return false;
    status->phase = WIFI_SCAN_SCANNING;
    return true;
}

bool wifi_scan_finish(wifi_scan_snapshot_t *status, uint16_t total, uint16_t reported)
{
    if (!valid_status(status) || status->phase != WIFI_SCAN_SCANNING || reported > total ||
        reported > WIFI_SCAN_MAX_RECORDS)
        return false;
    status->phase = WIFI_SCAN_IDLE;
    status->ap_count = total;
    status->reported_count = reported;
    if (status->completed_scans < UINT32_MAX)
        status->completed_scans++;
    return true;
}

bool wifi_scan_fail(wifi_scan_snapshot_t *status, int32_t error)
{
    if (!valid_status(status) || status->phase == WIFI_SCAN_OFF || status->phase == WIFI_SCAN_ERROR ||
        error == 0)
        return false;
    status->phase = WIFI_SCAN_ERROR;
    status->last_error = error;
    return true;
}

static bool prepare_buffer(char *out, size_t capacity)
{
    if (out == NULL || capacity == 0)
        return false;
    out[0] = '\0';
    return capacity <= INT_MAX;
}

static bool finish_json(cJSON *json, bool complete, char *out, size_t capacity)
{
    bool success = complete && cJSON_PrintPreallocated(json, out, (int)capacity, false);
    cJSON_Delete(json);
    if (!success)
        out[0] = '\0';
    return success;
}

bool wifi_scan_format_status(const wifi_scan_snapshot_t *status, char *out, size_t capacity)
{
    if (!prepare_buffer(out, capacity) || !valid_status(status))
        return false;
    cJSON *json = cJSON_CreateObject();
    if (json == NULL)
        return false;
    bool complete = cJSON_AddStringToObject(json, "state", wifi_scan_phase_name(status->phase)) != NULL &&
                    cJSON_AddBoolToObject(json, "connected", false) != NULL &&
                    cJSON_AddNullToObject(json, "ip") != NULL &&
                    cJSON_AddNullToObject(json, "rssi") != NULL &&
                    cJSON_AddNumberToObject(json, "completed_scans", status->completed_scans) != NULL &&
                    cJSON_AddNumberToObject(json, "ap_count", status->ap_count) != NULL &&
                    cJSON_AddNumberToObject(json, "reported_count", status->reported_count) != NULL;
    if (complete) {
        complete = status->phase == WIFI_SCAN_ERROR
                       ? cJSON_AddNumberToObject(json, "last_error", status->last_error) != NULL
                       : cJSON_AddNullToObject(json, "last_error") != NULL;
    }
    return finish_json(json, complete, out, capacity);
}

static const char *security_name(wifi_scan_security_t security)
{
    switch (security) {
    case WIFI_SCAN_AUTH_UNKNOWN:
        return "unknown";
    case WIFI_SCAN_AUTH_OPEN:
        return "open";
    case WIFI_SCAN_AUTH_WEP:
        return "wep";
    case WIFI_SCAN_AUTH_WPA:
        return "wpa_psk";
    case WIFI_SCAN_AUTH_WPA2:
        return "wpa2_psk";
    case WIFI_SCAN_AUTH_WPA_WPA2:
        return "wpa_wpa2_psk";
    case WIFI_SCAN_AUTH_WPA2_ENTERPRISE:
        return "wpa2_enterprise";
    case WIFI_SCAN_AUTH_WPA3:
        return "wpa3_psk";
    case WIFI_SCAN_AUTH_WPA2_WPA3:
        return "wpa2_wpa3_psk";
    case WIFI_SCAN_AUTH_OWE:
        return "owe";
    default:
        return NULL;
    }
}

bool wifi_scan_format_ap(const wifi_scan_ap_t *ap, char *out, size_t capacity)
{
    if (!prepare_buffer(out, capacity) || ap == NULL || ap->ssid_length > WIFI_SCAN_SSID_BYTES ||
        ap->channel < 1 || ap->channel > 11 || security_name(ap->security) == NULL || ap->authmode_id < 0)
        return false;
    const char digits[] = "0123456789abcdef";
    char ssid_hex[WIFI_SCAN_SSID_BYTES * 2 + 1];
    for (size_t i = 0; i < ap->ssid_length; i++) {
        ssid_hex[2 * i] = digits[ap->ssid[i] >> 4];
        ssid_hex[2 * i + 1] = digits[ap->ssid[i] & 15U];
    }
    ssid_hex[ap->ssid_length * 2] = '\0';
    char bssid[18];
    (void)snprintf(bssid, sizeof(bssid), "**:**:**:**:%02x:%02x", (unsigned)ap->bssid[4],
                   (unsigned)ap->bssid[5]);
    cJSON *json = cJSON_CreateObject();
    if (json == NULL)
        return false;
    bool complete = cJSON_AddStringToObject(json, "ssid_hex", ssid_hex) != NULL &&
                    cJSON_AddStringToObject(json, "bssid_masked", bssid) != NULL &&
                    cJSON_AddNumberToObject(json, "channel", ap->channel) != NULL &&
                    cJSON_AddNumberToObject(json, "rssi", ap->rssi) != NULL &&
                    cJSON_AddStringToObject(json, "security", security_name(ap->security)) != NULL &&
                    cJSON_AddNumberToObject(json, "authmode_id", ap->authmode_id) != NULL;
    return finish_json(json, complete, out, capacity);
}
