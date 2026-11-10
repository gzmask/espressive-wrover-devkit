#pragma once
#include "esp_err.h"
#include "lcd_terminal.h"
#include "lisp_app.h"
#include <stdbool.h>

/* Start the one-owner diagnostic display task, at most once per boot. */
esp_err_t board_display_start(void);
/* Recent successful frame submission, not a claim of visual inspection. */
bool board_display_is_healthy(void);
/* Copy a text snapshot; only the existing display owner touches SPI/DMA. */
esp_err_t board_display_terminal_update(const lcd_terminal_t *terminal);
/* Static owner backend; fixed PSRAM canvases, no direct SPI access. */
bool board_display_app_backend(lisp_app_display_t *backend);
