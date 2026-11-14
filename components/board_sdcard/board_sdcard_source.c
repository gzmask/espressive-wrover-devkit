#include "board_sdcard_source.h"

bool board_sdcard_source_slot_init(board_sdcard_source_slot_t *slot, char *buffer, size_t capacity)
{
    if (!slot || !buffer || capacity < BOARD_SDCARD_SOURCE_LIMIT + 1)
        return false;
    *slot = (board_sdcard_source_slot_t){.buffer = buffer, .capacity = capacity};
    return true;
}
bool board_sdcard_source_slot_begin(board_sdcard_source_slot_t *slot)
{
    if (!slot || !slot->buffer || slot->phase != BOARD_SOURCE_IDLE)
        return false;
    slot->cancelled = false;
    slot->length = 0;
    slot->phase = BOARD_SOURCE_FETCHING;
    return true;
}
void board_sdcard_source_slot_cancel(board_sdcard_source_slot_t *slot)
{
    slot->cancelled = true;
    if (slot->phase == BOARD_SOURCE_READY || slot->phase == BOARD_SOURCE_FAILED)
        slot->phase = BOARD_SOURCE_IDLE;
}
void board_sdcard_source_slot_finish(board_sdcard_source_slot_t *slot, bool success, size_t length)
{
    if (slot->phase != BOARD_SOURCE_FETCHING)
        return;
    slot->length = length;
    slot->phase = slot->cancelled                                            ? BOARD_SOURCE_IDLE
                  : success && length && length <= BOARD_SDCARD_SOURCE_LIMIT ? BOARD_SOURCE_READY
                                                                             : BOARD_SOURCE_FAILED;
}
board_sdcard_source_phase_t board_sdcard_source_slot_poll(board_sdcard_source_slot_t *slot,
                                                          const char **buffer, size_t *length)
{
    board_sdcard_source_phase_t phase = slot->phase;
    if (phase == BOARD_SOURCE_READY) {
        *buffer = slot->buffer;
        *length = slot->length;
        slot->phase = BOARD_SOURCE_LEASED;
    } else if (phase == BOARD_SOURCE_FAILED)
        slot->phase = BOARD_SOURCE_IDLE;
    return phase;
}
void board_sdcard_source_slot_release(board_sdcard_source_slot_t *slot)
{
    if (slot->phase == BOARD_SOURCE_LEASED)
        slot->phase = BOARD_SOURCE_IDLE;
}

bool board_sdcard_source_read(board_sdcard_source_read_fn read, board_sdcard_source_cancel_fn cancelled,
                              void *user, char *destination, size_t capacity, size_t *length)
{
    if (!length)
        return false;
    *length = 0;
    if (!read || !cancelled || !destination || capacity < BOARD_SDCARD_SOURCE_LIMIT + 1)
        return false;
    destination[0] = 0;
    size_t used = 0;
    for (;;) {
        if (cancelled(user))
            break;
        size_t room = capacity - used;
        if (room > BOARD_SDCARD_SOURCE_CHUNK)
            room = BOARD_SDCARD_SOURCE_CHUNK;
        /* Read at most one extra byte to detect an oversized file. */
        size_t remaining = BOARD_SDCARD_SOURCE_LIMIT + 1 - used;
        if (room > remaining)
            room = remaining;
        size_t count = 0;
        if (read(user, destination + used, room, &count) != 0 || count > room)
            break;
        if (count == 0) {
            if (used && !cancelled(user)) {
                destination[used] = 0;
                *length = used;
                return true;
            }
            break;
        }
        for (size_t i = 0; i < count; i++) {
            unsigned char value = (unsigned char)destination[used + i];
            if ((value < 32 && value != '\t' && value != '\r' && value != '\n') || value > 126) {
                destination[0] = 0;
                return false;
            }
        }
        used += count;
        if (used > BOARD_SDCARD_SOURCE_LIMIT)
            break;
    }
    destination[0] = 0;
    return false;
}
