#pragma once
#include "board_sdcard_diagnostics.h"
#include "board_sdcard_logic.h"
#include "board_sdcard_source.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "ff.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* A single worker owns this persistent workspace, including the configuration-sized
 * FIL sector cache. Never put it on the task stack or reuse a quarantined handle. */
typedef struct {
    FIL file;
    char path[BOARD_SDCARD_MOUNT_PATH_MAX];
    char data[BOARD_SDCARD_READ_MAX + 1];
    size_t length;
    bool open;
    bool quarantined;
} board_sdcard_file_t;
typedef void (*board_sdcard_file_trace_t)(void *user, board_sdcard_phase_t phase, int error, int fat_result,
                                          size_t transferred, bool quarantined);
int board_sdcard_file_read(board_sdcard_file_t *workspace, const char *drive, const char *name,
                           board_sdcard_file_trace_t trace, void *user);
int board_sdcard_file_write(board_sdcard_file_t *workspace, const char *drive, const char *name,
                            const char *text, size_t length, board_sdcard_file_trace_t trace, void *user);
/* Internal loader path: caller owns a persistent buffer until worker completes.
 * Closing failure quarantines the handle; never evaluate partial source. */
int board_sdcard_file_read_source(board_sdcard_file_t *workspace, const char *drive, const char *name,
                                  char *source, size_t capacity, size_t *length,
                                  board_sdcard_source_cancel_fn cancelled, void *cancel_user,
                                  board_sdcard_file_trace_t trace, void *trace_user);
