#pragma once
#include "esp_err.h"
#include "wifi_link_logic.h"
esp_err_t node_wifi_station_start(void);
bool node_wifi_station_get_snapshot(wifi_link_snapshot_t *out);
