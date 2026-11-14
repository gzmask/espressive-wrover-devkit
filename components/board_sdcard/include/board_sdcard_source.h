#pragma once
#include <stdbool.h>
#include <stddef.h>

enum { BOARD_SDCARD_SOURCE_LIMIT = 32768, BOARD_SDCARD_SOURCE_CHUNK = 512 };
/* Internal helper only; callbacks own their resources and must close them.
 * No source is considered valid until this function returns true. */
typedef int (*board_sdcard_source_read_fn)(void *user, char *buffer, size_t capacity, size_t *count);
typedef bool (*board_sdcard_source_cancel_fn)(void *user);
bool board_sdcard_source_read(board_sdcard_source_read_fn read, board_sdcard_source_cancel_fn cancelled,
                              void *user, char *destination, size_t capacity, size_t *length);

/* Caller serializes slot metadata. A cancelled fetching buffer remains owned by
 * the worker until finish; READY is exclusively borrowed until release. */
typedef enum {
    BOARD_SOURCE_IDLE,
    BOARD_SOURCE_FETCHING,
    BOARD_SOURCE_READY,
    BOARD_SOURCE_FAILED,
    BOARD_SOURCE_LEASED
} board_sdcard_source_phase_t;
typedef struct {
    board_sdcard_source_phase_t phase;
    bool cancelled;
    char *buffer;
    size_t capacity, length;
} board_sdcard_source_slot_t;
bool board_sdcard_source_slot_init(board_sdcard_source_slot_t *slot, char *buffer, size_t capacity);
bool board_sdcard_source_slot_begin(board_sdcard_source_slot_t *slot);
void board_sdcard_source_slot_cancel(board_sdcard_source_slot_t *slot);
void board_sdcard_source_slot_finish(board_sdcard_source_slot_t *slot, bool success, size_t length);
board_sdcard_source_phase_t board_sdcard_source_slot_poll(board_sdcard_source_slot_t *slot,
                                                          const char **buffer, size_t *length);
void board_sdcard_source_slot_release(board_sdcard_source_slot_t *slot);
