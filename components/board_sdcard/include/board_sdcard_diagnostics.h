#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { BOARD_SDCARD_DIAGNOSTIC_MAX = 256 };
typedef enum {
    BOARD_SD_IDLE,
    BOARD_SD_QUEUED,
    BOARD_SD_SPI_BUS_INIT,
    BOARD_SD_HOST_INIT,
    BOARD_SD_DEVICE_INIT,
    BOARD_SD_CARD_ALLOC,
    BOARD_SD_CARD_INIT,
    BOARD_SD_DRIVE_ALLOC,
    BOARD_SD_PATH,
    BOARD_SD_FAT_MOUNT,
    BOARD_SD_OPEN_READ,
    BOARD_SD_READ,
    BOARD_SD_VALIDATE_READ,
    BOARD_SD_OPEN_WRITE,
    BOARD_SD_WRITE,
    BOARD_SD_SYNC,
    BOARD_SD_CLOSE,
    BOARD_SD_FAT_UNMOUNT,
    BOARD_SD_DEVICE_REMOVE,
    BOARD_SD_BUS_FREE,
    BOARD_SD_LEASE_RELEASE,
    BOARD_SD_COMPLETE
} board_sdcard_phase_t;
typedef struct {
    board_sdcard_phase_t phase;
    board_sdcard_phase_t failed_phase;
    int error;
    int fat_result;          /* -1 means no FatFs return observed. First failure wins. */
    int close_result;        /* Independent cleanup result; -1 means not attempted. */
    uint32_t stack_min_free; /* Sampled worker lifetime low-water bytes; UINT32_MAX = unknown. */
    uint32_t sequence;
    size_t requested;
    size_t transferred;
    bool quarantined;
} board_sdcard_diagnostic_t;

const char *board_sdcard_phase_name(board_sdcard_phase_t phase);
void board_sdcard_diagnostic_begin(board_sdcard_diagnostic_t *diagnostic, size_t requested);
void board_sdcard_diagnostic_record(board_sdcard_diagnostic_t *diagnostic, board_sdcard_phase_t phase,
                                    int error, int fat_result, uint32_t stack_free, size_t transferred,
                                    bool quarantined);
bool board_sdcard_diagnostic_format(const board_sdcard_diagnostic_t *diagnostic, char *text, size_t capacity);
