#pragma once
#include "esp_err.h"
#include <stdbool.h>

/* Start the one-owner diagnostic display task, at most once per boot. */
esp_err_t board_display_start(void);
/* Recent successful frame submission, not a claim of visual inspection. */
bool board_display_is_healthy(void);
