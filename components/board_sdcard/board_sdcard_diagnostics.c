#include "board_sdcard_diagnostics.h"
#include <inttypes.h>
#include <stdio.h>

const char *board_sdcard_phase_name(board_sdcard_phase_t phase)
{
    static const char *const names[] = {
        "idle",          "queued",      "spi-bus-init",  "host-init", "device-init", "card-alloc",
        "card-init",     "drive-alloc", "path",          "fat-mount", "open-read",   "read",
        "validate-read", "open-write",  "write",         "sync",      "close",       "fat-unmount",
        "device-remove", "bus-free",    "lease-release", "complete"};
    return (unsigned)phase < sizeof(names) / sizeof(names[0]) ? names[phase] : "unknown";
}
void board_sdcard_diagnostic_begin(board_sdcard_diagnostic_t *diagnostic, size_t requested)
{
    uint32_t next = diagnostic->sequence == UINT32_MAX ? UINT32_MAX : diagnostic->sequence + 1;
    *diagnostic = (board_sdcard_diagnostic_t){.phase = BOARD_SD_QUEUED,
                                              .fat_result = -1,
                                              .close_result = -1,
                                              .stack_min_free = UINT32_MAX,
                                              .sequence = next,
                                              .requested = requested};
}
void board_sdcard_diagnostic_record(board_sdcard_diagnostic_t *diagnostic, board_sdcard_phase_t phase,
                                    int error, int fat_result, uint32_t stack_free, size_t transferred,
                                    bool quarantined)
{
    diagnostic->phase = phase;
    if (diagnostic->failed_phase == BOARD_SD_IDLE) {
        if (fat_result >= 0)
            diagnostic->fat_result = fat_result;
        if (error != 0) {
            diagnostic->failed_phase = phase;
            diagnostic->error = error;
        }
    }
    if (phase == BOARD_SD_CLOSE && fat_result >= 0)
        diagnostic->close_result = fat_result;
    if (stack_free < diagnostic->stack_min_free)
        diagnostic->stack_min_free = stack_free;
    diagnostic->transferred = transferred;
    diagnostic->quarantined = diagnostic->quarantined || quarantined;
}
bool board_sdcard_diagnostic_format(const board_sdcard_diagnostic_t *diagnostic, char *text, size_t capacity)
{
    if (!diagnostic || !text || capacity == 0)
        return false;
    char stack[16];
    if (diagnostic->stack_min_free == UINT32_MAX)
        (void)snprintf(stack, sizeof(stack), "unknown");
    else
        (void)snprintf(stack, sizeof(stack), "%" PRIu32, diagnostic->stack_min_free);
    int count = snprintf(text, capacity,
                         "phase=%s failed=%s error=%d fat=%d close=%d stack_min=%s bytes=%zu/%zu seq=%" PRIu32
                         " quarantine=%u",
                         board_sdcard_phase_name(diagnostic->phase),
                         board_sdcard_phase_name(diagnostic->failed_phase), diagnostic->error,
                         diagnostic->fat_result, diagnostic->close_result, stack, diagnostic->transferred,
                         diagnostic->requested, diagnostic->sequence, diagnostic->quarantined ? 1u : 0u);
    if (count < 0 || (size_t)count >= capacity) {
        text[0] = 0;
        return false;
    }
    return true;
}
