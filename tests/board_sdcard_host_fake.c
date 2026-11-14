#include "board_sdcard_host_fake.h"
#include "board_sdcard.h"
#include <assert.h>
#include <string.h>

static board_sdcard_state_t state = BOARD_SDCARD_DISABLED;
static display_lifecycle_state_t display = DISPLAY_ON;
static bool lease;
static int error_code;
static board_sdcard_diagnostic_t diagnostic;
static char result[BOARD_SDCARD_TEXT_MAX + 1];
static size_t result_length;
static unsigned requests_by_state[BOARD_SDCARD_FAULT + 1];
void board_sdcard_fake_reset(void)
{
    state = BOARD_SDCARD_DISABLED;
    display = DISPLAY_ON;
    lease = false;
    error_code = 0;
    diagnostic =
        (board_sdcard_diagnostic_t){.fat_result = -1, .close_result = -1, .stack_min_free = UINT32_MAX};
    result_length = 0;
    memset(result, 0, sizeof(result));
    memset(requests_by_state, 0, sizeof(requests_by_state));
}
void board_sdcard_get_diagnostics(board_sdcard_diagnostic_t *destination)
{
    if (destination)
        *destination = diagnostic;
}
void board_sdcard_fake_set_diagnostics(const board_sdcard_diagnostic_t *source)
{
    assert(source != NULL);
    diagnostic = *source;
}
void board_sdcard_fake_set_display(display_lifecycle_state_t next)
{
    display = next;
}
void board_sdcard_fake_complete_mount(bool success, int error)
{
    state = board_sdcard_mount_complete(success);
    error_code = error;
    if (!success)
        lease = false;
}
void board_sdcard_fake_complete_unmount(bool success, int error)
{
    state = board_sdcard_unmount_complete(success);
    error_code = error;
    if (success)
        lease = false;
}
void board_sdcard_fake_complete_read(const char *text, size_t length, int error)
{
    error_code = error;
    if (error != 0) {
        state = board_sdcard_io_complete(true, false, diagnostic.quarantined);
        return;
    }
    assert(length <= BOARD_SDCARD_READ_MAX && (length == 0 || text != NULL));
    if (length > 0)
        memcpy(result, text, length);
    result[length] = 0;
    result_length = length;
    state = board_sdcard_io_complete(true, true, diagnostic.quarantined);
}
void board_sdcard_fake_complete_write(bool success, int error)
{
    state = board_sdcard_io_complete(false, success, diagnostic.quarantined);
    error_code = error;
}
unsigned board_sdcard_fake_request_count(board_sdcard_state_t requested)
{
    return requests_by_state[requested];
}
int board_sdcard_start(void)
{
    state = BOARD_SDCARD_UNMOUNTED;
    return 0;
}
bool board_sdcard_request_mount(void)
{
    if (!board_sdcard_can_mount(state, display, !lease))
        return false;
    lease = true;
    state = BOARD_SDCARD_MOUNTING;
    requests_by_state[state]++;
    return true;
}
bool board_sdcard_request_unmount(void)
{
    if (!board_sdcard_can_unmount(state))
        return false;
    state = BOARD_SDCARD_UNMOUNTING;
    requests_by_state[state]++;
    return true;
}
bool board_sdcard_request_read(const char *name, size_t length)
{
    if (!board_sdcard_can_file_io(state) || !board_sdcard_filename_valid(name, length))
        return false;
    state = BOARD_SDCARD_READING;
    requests_by_state[state]++;
    return true;
}
bool board_sdcard_request_write(const char *name, size_t name_length, const char *text, size_t text_length)
{
    if (!board_sdcard_can_file_io(state) || !board_sdcard_filename_valid(name, name_length) ||
        !board_sdcard_text_valid(text, text_length))
        return false;
    state = BOARD_SDCARD_WRITING;
    requests_by_state[state]++;
    return true;
}
board_sdcard_state_t board_sdcard_state(void)
{
    return state;
}
int board_sdcard_last_error(void)
{
    return error_code;
}
size_t board_sdcard_read_result_size(void)
{
    return state == BOARD_SDCARD_READ_READY ? result_length : 0;
}
bool board_sdcard_take_read_result(char *destination, size_t capacity, size_t *length)
{
    if (!destination || !length || state != BOARD_SDCARD_READ_READY || capacity <= result_length)
        return false;
    memcpy(destination, result, result_length + 1);
    *length = result_length;
    result_length = 0;
    state = board_sdcard_result_consumed(state);
    return true;
}
bool board_sdcard_take_operation_result(board_sdcard_state_t *completed)
{
    if (!completed || (state != BOARD_SDCARD_WRITE_DONE && state != BOARD_SDCARD_OPERATION_ERROR))
        return false;
    *completed = state;
    state = board_sdcard_result_consumed(state);
    return true;
}
