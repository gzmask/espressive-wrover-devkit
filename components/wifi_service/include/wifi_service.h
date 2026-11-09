#pragma once
#include "esp_err.h"
#include "wifi_link_logic.h"
#include "wifi_scan_logic.h"

/* One owner, mutually exclusive passive-scan or opt-in station mode.
 * Station credentials are embedded from a private file, never saved to NVS.
 * Fatal errors are terminal until reset; LCD/heartbeat remain independent.
 * Do not call other Wi-Fi control APIs. Neither mode starts HTTP. */
esp_err_t wifi_service_start(void);
/* Short internal-RAM critical section; returns a value copy, never a pointer. */
bool wifi_service_get_snapshot(wifi_scan_snapshot_t *out);
bool wifi_service_get_link_snapshot(wifi_link_snapshot_t *out);
