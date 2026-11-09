#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { WIFI_SCAN_MAX_RECORDS = 16, WIFI_SCAN_SSID_BYTES = 32 };
typedef enum {
    WIFI_SCAN_OFF,
    WIFI_SCAN_STARTING,
    WIFI_SCAN_SCANNING,
    WIFI_SCAN_IDLE,
    WIFI_SCAN_ERROR
} wifi_scan_phase_t;
typedef enum {
    WIFI_SCAN_AUTH_UNKNOWN,
    WIFI_SCAN_AUTH_OPEN,
    WIFI_SCAN_AUTH_WEP,
    WIFI_SCAN_AUTH_WPA,
    WIFI_SCAN_AUTH_WPA2,
    WIFI_SCAN_AUTH_WPA_WPA2,
    WIFI_SCAN_AUTH_WPA2_ENTERPRISE,
    WIFI_SCAN_AUTH_WPA3,
    WIFI_SCAN_AUTH_WPA2_WPA3,
    WIFI_SCAN_AUTH_OWE
} wifi_scan_security_t;
typedef struct {
    wifi_scan_phase_t phase;
    uint32_t completed_scans;
    uint16_t ap_count;
    uint16_t reported_count;
    int32_t last_error;
} wifi_scan_snapshot_t;
typedef struct {
    uint8_t ssid[WIFI_SCAN_SSID_BYTES];
    size_t ssid_length;
    uint8_t bssid[6];
    uint8_t channel;
    int8_t rssi;
    wifi_scan_security_t security;
    int32_t authmode_id;
} wifi_scan_ap_t;

wifi_scan_snapshot_t wifi_scan_initial(void);
const char *wifi_scan_phase_name(wifi_scan_phase_t phase);
bool wifi_scan_starting(wifi_scan_snapshot_t *status);
bool wifi_scan_begin(wifi_scan_snapshot_t *status);
bool wifi_scan_finish(wifi_scan_snapshot_t *status, uint16_t total, uint16_t reported);
bool wifi_scan_fail(wifi_scan_snapshot_t *status, int32_t error);
/* Secrets are not inputs. SSID is hex-encoded (not anonymized); BSSID is masked.
 * Any formatting failure empties the caller's buffer when possible. */
bool wifi_scan_format_status(const wifi_scan_snapshot_t *status, char *out, size_t capacity);
bool wifi_scan_format_ap(const wifi_scan_ap_t *ap, char *out, size_t capacity);
