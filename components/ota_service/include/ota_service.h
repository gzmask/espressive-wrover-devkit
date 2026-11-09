#pragma once
#include "esp_err.h"
#include <stdbool.h>

/* Callback reads published health only; never takes ownership of peripherals.
 * Starts at most once. No flash write until an authenticated upload, except
 * pending-boot confirmation in the separately approved otadata range. */
esp_err_t ota_service_start(bool (*healthy)(void));
