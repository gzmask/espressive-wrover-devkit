#pragma once
#include "board_sdcard_diagnostics.h"
#include "board_sdcard_logic.h"
#include "board_sdcard_source.h"
#include <stdbool.h>
#include <stddef.h>

/* The worker owns the SPI/FATFS device. Requests are copied into a one-entry queue. */
int board_sdcard_start(void);
bool board_sdcard_request_mount(void);
bool board_sdcard_request_unmount(void);
bool board_sdcard_request_read(const char *name, size_t name_length);
bool board_sdcard_request_write(const char *name, size_t name_length, const char *text, size_t text_length);
/* Source-loading lease is independent of ordinary 240-byte sd-read. */
bool board_sdcard_source_request(const char *name, size_t length);
board_sdcard_source_phase_t board_sdcard_source_poll(const char **source, size_t *length);
void board_sdcard_source_cancel(void);
void board_sdcard_source_release(void);
board_sdcard_state_t board_sdcard_state(void);
int board_sdcard_last_error(void);
void board_sdcard_get_diagnostics(board_sdcard_diagnostic_t *diagnostic);
size_t board_sdcard_read_result_size(void);
bool board_sdcard_take_read_result(char *destination, size_t capacity, size_t *length);
bool board_sdcard_take_operation_result(board_sdcard_state_t *completed_state);
