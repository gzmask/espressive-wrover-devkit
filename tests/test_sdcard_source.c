#include "board_sdcard_source.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char source[BOARD_SDCARD_SOURCE_LIMIT + 1];
static char destination[BOARD_SDCARD_SOURCE_LIMIT + 1];
typedef struct {
    size_t length, offset;
    unsigned calls, cancel_at;
    bool fail;
} fake_t;
static int read_chunk(void *user, char *out, size_t capacity, size_t *count)
{
    fake_t *fake = user;
    fake->calls++;
    if (fake->fail)
        return -1;
    size_t remaining = fake->length - fake->offset;
    *count = remaining < capacity ? remaining : capacity;
    memcpy(out, source + fake->offset, *count);
    fake->offset += *count;
    return 0;
}
static bool cancelled(void *user)
{
    fake_t *fake = user;
    return fake->cancel_at && fake->calls >= fake->cancel_at;
}
int main(void)
{
    memset(source, 'x', sizeof(source));
    fake_t fake = {.length = BOARD_SDCARD_SOURCE_LIMIT};
    size_t length = 0;
    assert(board_sdcard_source_read(read_chunk, cancelled, &fake, destination, sizeof(destination), &length));
    assert(length == BOARD_SDCARD_SOURCE_LIMIT && destination[length] == 0);
    assert(!memcmp(source, destination, length));
    fake = (fake_t){.length = BOARD_SDCARD_SOURCE_LIMIT + 1};
    assert(
        !board_sdcard_source_read(read_chunk, cancelled, &fake, destination, sizeof(destination), &length));
    assert(length == 0 && destination[0] == 0);
    fake = (fake_t){.length = 0};
    assert(
        !board_sdcard_source_read(read_chunk, cancelled, &fake, destination, sizeof(destination), &length));
    fake = (fake_t){.length = 1024, .cancel_at = 1};
    assert(
        !board_sdcard_source_read(read_chunk, cancelled, &fake, destination, sizeof(destination), &length));
    fake = (fake_t){.length = 2, .fail = true};
    assert(
        !board_sdcard_source_read(read_chunk, cancelled, &fake, destination, sizeof(destination), &length));
    fake = (fake_t){.length = 3};
    source[1] = 0;
    assert(
        !board_sdcard_source_read(read_chunk, cancelled, &fake, destination, sizeof(destination), &length));
    assert(length == 0 && destination[0] == 0);
    assert(!board_sdcard_source_read(read_chunk, cancelled, &fake, destination, 1, &length));
    board_sdcard_source_slot_t slot;
    assert(board_sdcard_source_slot_init(&slot, destination, sizeof(destination)));
    assert(board_sdcard_source_slot_begin(&slot));
    board_sdcard_source_slot_cancel(&slot);
    assert(!board_sdcard_source_slot_begin(&slot)); /* worker still owns buffer */
    board_sdcard_source_slot_finish(&slot, true, 10);
    assert(slot.phase == BOARD_SOURCE_IDLE);
    assert(board_sdcard_source_slot_begin(&slot));
    board_sdcard_source_slot_finish(&slot, true, 10);
    const char *borrowed = NULL;
    assert(board_sdcard_source_slot_poll(&slot, &borrowed, &length) == BOARD_SOURCE_READY);
    assert(borrowed == destination && length == 10 && slot.phase == BOARD_SOURCE_LEASED);
    assert(!board_sdcard_source_slot_begin(&slot));
    board_sdcard_source_slot_release(&slot);
    assert(board_sdcard_source_slot_begin(&slot));
    board_sdcard_source_slot_finish(&slot, false, 0);
    assert(board_sdcard_source_slot_poll(&slot, &borrowed, &length) == BOARD_SOURCE_FAILED);
    assert(slot.phase == BOARD_SOURCE_IDLE);
    assert(board_sdcard_source_slot_begin(&slot));
    board_sdcard_source_slot_finish(&slot, true, 4);
    board_sdcard_source_slot_cancel(&slot);
    assert(slot.phase == BOARD_SOURCE_IDLE && board_sdcard_source_slot_begin(&slot));
    board_sdcard_source_slot_finish(&slot, true, BOARD_SDCARD_SOURCE_LIMIT + 1);
    assert(board_sdcard_source_slot_poll(&slot, &borrowed, &length) == BOARD_SOURCE_FAILED);
    assert(slot.phase == BOARD_SOURCE_IDLE);
    puts("PASS bounded source read and slot ownership/cancel; pure mock, no device");
    return 0;
}
