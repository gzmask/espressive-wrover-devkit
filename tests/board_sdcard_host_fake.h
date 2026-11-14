#pragma once
#include "board_sdcard.h"
void board_sdcard_fake_reset(void);
void board_sdcard_fake_set_diagnostics(const board_sdcard_diagnostic_t *diagnostic);
void board_sdcard_fake_set_display(display_lifecycle_state_t state);
void board_sdcard_fake_complete_mount(bool success, int error);
void board_sdcard_fake_complete_unmount(bool success, int error);
void board_sdcard_fake_complete_read(const char *text, size_t length, int error);
void board_sdcard_fake_complete_write(bool success, int error);
unsigned board_sdcard_fake_request_count(board_sdcard_state_t state);
