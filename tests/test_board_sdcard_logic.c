#include "board_sdcard_logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool valid_name(const char *name)
{
    return board_sdcard_filename_valid(name, strlen(name));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "names")) {
        assert(valid_name("note.txt"));
        assert(valid_name("A_1-test.LOG"));
        char maximum[BOARD_SDCARD_FILENAME_MAX + 1];
        memset(maximum, 'a', BOARD_SDCARD_FILENAME_MAX);
        maximum[sizeof(maximum) - 1] = 0;
        assert(valid_name(maximum));
        const char *invalid[] = {"",    ".",    "..",        ".hidden", "trailing.", "two..dots", "../escape",
                                 "a/b", "a\\b", "/absolute", "a:b",     "a b",       "a\n",       "ü.txt"};
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
            assert(!valid_name(invalid[i]));
        maximum[BOARD_SDCARD_FILENAME_MAX] = 'a';
        assert(!board_sdcard_filename_valid(maximum, sizeof(maximum)));
    } else if (!strcmp(argv[1], "text")) {
        assert(board_sdcard_text_valid("", 0));
        assert(board_sdcard_text_valid("line one\nline two\r\n\t", 20));
        char maximum[BOARD_SDCARD_TEXT_MAX];
        memset(maximum, 'x', sizeof(maximum));
        assert(board_sdcard_text_valid(maximum, sizeof(maximum)));
        assert(!board_sdcard_text_valid(maximum, sizeof(maximum) + 1));
        assert(!board_sdcard_text_valid("bad\0text", 8));
        assert(!board_sdcard_text_valid("bad\001", 4));
        assert(!board_sdcard_text_valid("bad\177", 4));
        assert(!board_sdcard_text_valid("bad\200", 4));
    } else if (!strcmp(argv[1], "mount-policy")) {
        assert(board_sdcard_can_mount(BOARD_SDCARD_UNMOUNTED, DISPLAY_OFF, true));
        assert(board_sdcard_can_mount(BOARD_SDCARD_UNMOUNTED, DISPLAY_SD_BLOCKED, true));
        assert(!board_sdcard_can_mount(BOARD_SDCARD_UNMOUNTED, DISPLAY_ON, true));
        assert(!board_sdcard_can_mount(BOARD_SDCARD_UNMOUNTED, DISPLAY_STARTING, true));
        assert(!board_sdcard_can_mount(BOARD_SDCARD_UNMOUNTED, DISPLAY_STOPPING, true));
        assert(!board_sdcard_can_mount(BOARD_SDCARD_UNMOUNTED, DISPLAY_FAULT, true));
        assert(!board_sdcard_can_mount(BOARD_SDCARD_UNMOUNTED, DISPLAY_OFF, false));
        assert(!board_sdcard_can_mount(BOARD_SDCARD_MOUNTED, DISPLAY_OFF, true));
        assert(board_sdcard_can_unmount(BOARD_SDCARD_MOUNTED));
        assert(!board_sdcard_can_unmount(BOARD_SDCARD_READING));
        assert(!board_sdcard_can_unmount(BOARD_SDCARD_WRITING));
        assert(board_sdcard_can_file_io(BOARD_SDCARD_MOUNTED));
        for (int state = BOARD_SDCARD_DISABLED; state <= BOARD_SDCARD_FAULT; state++)
            if (state != BOARD_SDCARD_MOUNTED)
                assert(!board_sdcard_can_file_io((board_sdcard_state_t)state));
    } else if (!strcmp(argv[1], "transitions")) {
        assert(board_sdcard_mount_complete(true) == BOARD_SDCARD_MOUNTED);
        assert(board_sdcard_mount_complete(false) == BOARD_SDCARD_UNMOUNTED);
        assert(board_sdcard_unmount_complete(true) == BOARD_SDCARD_UNMOUNTED);
        assert(board_sdcard_unmount_complete(false) == BOARD_SDCARD_FAULT);
        assert(board_sdcard_io_complete(true, true, false) == BOARD_SDCARD_READ_READY);
        assert(board_sdcard_io_complete(false, true, false) == BOARD_SDCARD_WRITE_DONE);
        assert(board_sdcard_io_complete(true, false, false) == BOARD_SDCARD_OPERATION_ERROR);
        assert(board_sdcard_io_complete(true, false, true) == BOARD_SDCARD_FAULT);
        assert(board_sdcard_io_complete(false, false, true) == BOARD_SDCARD_FAULT);
        assert(board_sdcard_result_consumed(BOARD_SDCARD_FAULT) == BOARD_SDCARD_FAULT);
        assert(!board_sdcard_can_unmount(BOARD_SDCARD_FAULT));
        assert(board_sdcard_result_consumed(BOARD_SDCARD_READ_READY) == BOARD_SDCARD_MOUNTED);
        assert(board_sdcard_result_consumed(BOARD_SDCARD_WRITE_DONE) == BOARD_SDCARD_MOUNTED);
        assert(board_sdcard_result_consumed(BOARD_SDCARD_OPERATION_ERROR) == BOARD_SDCARD_MOUNTED);
        assert(board_sdcard_result_consumed(BOARD_SDCARD_READING) == BOARD_SDCARD_READING);
        assert(board_sdcard_mount_complete(false) != BOARD_SDCARD_FAULT); /* failed mount can release lease */
    } else {
        return 1;
    }
    puts("PASS");
    return 0;
}
