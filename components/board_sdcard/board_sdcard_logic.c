#include "board_sdcard_logic.h"

static bool ascii_alphanumeric(unsigned char value)
{
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9');
}
bool board_sdcard_filename_valid(const char *name, size_t length)
{
    if (!name || length == 0 || length > BOARD_SDCARD_FILENAME_MAX || name[0] == '.' ||
        name[length - 1] == '.')
        return false;
    bool has_extension_separator = false;
    for (size_t i = 0; i < length; i++) {
        unsigned char value = (unsigned char)name[i];
        if (ascii_alphanumeric(value) || value == '_' || value == '-')
            continue;
        if (value != '.' || has_extension_separator || i == 0 || name[i - 1] == '.')
            return false;
        has_extension_separator = true;
    }
    return true;
}
bool board_sdcard_text_valid(const char *text, size_t length)
{
    if (length > BOARD_SDCARD_TEXT_MAX || (length != 0 && !text))
        return false;
    for (size_t i = 0; i < length; i++) {
        unsigned char value = (unsigned char)text[i];
        if (value == '\n' || value == '\r' || value == '\t' || (value >= 32 && value <= 126))
            continue;
        return false;
    }
    return true;
}
bool board_sdcard_can_mount(board_sdcard_state_t state, display_lifecycle_state_t display_state,
                            bool lease_available)
{
    bool display_off = display_state == DISPLAY_OFF || display_state == DISPLAY_SD_BLOCKED;
    return state == BOARD_SDCARD_UNMOUNTED && display_off && lease_available;
}
bool board_sdcard_can_unmount(board_sdcard_state_t state)
{
    return state == BOARD_SDCARD_MOUNTED;
}
bool board_sdcard_can_file_io(board_sdcard_state_t state)
{
    return state == BOARD_SDCARD_MOUNTED;
}
board_sdcard_state_t board_sdcard_mount_complete(bool success)
{
    return success ? BOARD_SDCARD_MOUNTED : BOARD_SDCARD_UNMOUNTED;
}
board_sdcard_state_t board_sdcard_unmount_complete(bool success)
{
    return success ? BOARD_SDCARD_UNMOUNTED : BOARD_SDCARD_FAULT;
}
board_sdcard_state_t board_sdcard_io_complete(bool reading, bool success, bool quarantined)
{
    if (quarantined)
        return BOARD_SDCARD_FAULT;
    if (!success)
        return BOARD_SDCARD_OPERATION_ERROR;
    return reading ? BOARD_SDCARD_READ_READY : BOARD_SDCARD_WRITE_DONE;
}
board_sdcard_state_t board_sdcard_result_consumed(board_sdcard_state_t state)
{
    if (state == BOARD_SDCARD_READ_READY || state == BOARD_SDCARD_WRITE_DONE ||
        state == BOARD_SDCARD_OPERATION_ERROR)
        return BOARD_SDCARD_MOUNTED;
    return state;
}
