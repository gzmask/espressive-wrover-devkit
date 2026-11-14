#pragma once
#include "display_lifecycle.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    BOARD_SDCARD_FILENAME_MAX = 32,
    BOARD_SDCARD_TEXT_MAX = 512,
    BOARD_SDCARD_READ_MAX = 240,
    BOARD_SDCARD_MOUNT_PATH_MAX = 48,
};

typedef enum {
    BOARD_SDCARD_DISABLED,
    BOARD_SDCARD_UNMOUNTED,
    BOARD_SDCARD_MOUNTING,
    BOARD_SDCARD_MOUNTED,
    BOARD_SDCARD_READING,
    BOARD_SDCARD_WRITING,
    BOARD_SDCARD_READ_READY,
    BOARD_SDCARD_WRITE_DONE,
    BOARD_SDCARD_OPERATION_ERROR,
    BOARD_SDCARD_UNMOUNTING,
    BOARD_SDCARD_FAULT
} board_sdcard_state_t;

bool board_sdcard_filename_valid(const char *name, size_t length);
bool board_sdcard_text_valid(const char *text, size_t length);
bool board_sdcard_can_mount(board_sdcard_state_t state, display_lifecycle_state_t display_state,
                            bool lease_available);
bool board_sdcard_can_unmount(board_sdcard_state_t state);
bool board_sdcard_can_file_io(board_sdcard_state_t state);
board_sdcard_state_t board_sdcard_mount_complete(bool success);
board_sdcard_state_t board_sdcard_unmount_complete(bool success);
board_sdcard_state_t board_sdcard_io_complete(bool reading, bool success, bool quarantined);
board_sdcard_state_t board_sdcard_result_consumed(board_sdcard_state_t state);
