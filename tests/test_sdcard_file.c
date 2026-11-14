#include "board_sdcard_file.h"
#include "esp_err.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static board_sdcard_file_t workspace; /* Actual IDF FIL with a 4KiB cache, never on stack. */
static board_sdcard_diagnostic_t diagnostic;
static const char *read_data = "hello\n";
static size_t read_size = 6;
static size_t read_offset;
static char source_buffer[BOARD_SDCARD_SOURCE_LIMIT + 1];
static bool never_cancel(void *user)
{
    (void)user;
    return false;
}
static FRESULT fail_open, fail_read, fail_write, fail_sync, fail_close;
static bool short_write;
static char calls[32];
static size_t used;
static void call(char value)
{
    assert(used + 1 < sizeof(calls));
    calls[used++] = value;
    calls[used] = 0;
}
FRESULT f_open(FIL *file, const TCHAR *path, BYTE mode)
{
    call('O');
    assert(file == &workspace.file && path == workspace.path);
    assert(!strcmp(path, "0:/NOTE.TXT"));
    assert(mode == FA_READ || mode == (FA_WRITE | FA_CREATE_ALWAYS));
    memset(file, 0xa5, sizeof(*file));
    return fail_open;
}
FRESULT f_read(FIL *file, void *data, UINT size, UINT *count)
{
    call('R');
    assert(file == &workspace.file);
    if (data != workspace.data) {
        assert(size <= BOARD_SDCARD_SOURCE_CHUNK);
        size_t remaining = read_size - read_offset;
        size_t amount = remaining < size ? remaining : size;
        memcpy(data, read_data + read_offset, amount);
        read_offset += amount;
        *count = (UINT)amount;
    } else {
        assert(size == BOARD_SDCARD_READ_MAX + 1 && read_size <= size);
        memcpy(data, read_data, read_size);
        *count = (UINT)read_size;
    }
    return fail_read;
}
FRESULT f_write(FIL *file, const void *data, UINT size, UINT *count)
{
    call('W');
    assert(file == &workspace.file && size <= BOARD_SDCARD_TEXT_MAX);
    assert(size == 0 || data != NULL);
    *count = short_write && size > 0 ? size - 1 : size;
    return fail_write;
}
FRESULT f_sync(FIL *file)
{
    call('S');
    assert(file == &workspace.file);
    return fail_sync;
}
FRESULT f_close(FIL *file)
{
    call('C');
    assert(file == &workspace.file);
    return fail_close;
}
static void trace(void *context, board_sdcard_phase_t phase, int error, int fat, size_t count,
                  bool quarantine)
{
    assert(context == &diagnostic);
    board_sdcard_diagnostic_record(context, phase, error, fat, 4096, count, quarantine);
}
int main(int argc, char **argv)
{
    assert(argc == 2 && sizeof(FIL) >= 4096);
    board_sdcard_diagnostic_begin(&diagnostic, 4);
    int error;
    if (!strcmp(argv[1], "source-close-error")) {
        fail_close = FR_DISK_ERR;
        size_t source_length = 0;
        error =
            board_sdcard_file_read_source(&workspace, "0:", "NOTE.TXT", source_buffer, sizeof(source_buffer),
                                          &source_length, never_cancel, NULL, trace, &diagnostic);
        assert(error != ESP_OK && source_length == 0 && source_buffer[0] == 0);
        assert(workspace.open && workspace.quarantined);
        assert(diagnostic.failed_phase == BOARD_SD_CLOSE && diagnostic.close_result == FR_DISK_ERR);
        assert(!strcmp(calls, "ORRC"));
    } else if (!strcmp(argv[1], "bounds")) {
        assert(board_sdcard_file_read(NULL, "0:", "NOTE.TXT", trace, &diagnostic) == ESP_ERR_INVALID_STATE);
        assert(board_sdcard_file_read(&workspace, "0:", "../bad", trace, &diagnostic) == ESP_ERR_INVALID_ARG);
        assert(board_sdcard_file_read(&workspace, "", "NOTE.TXT", trace, &diagnostic) == ESP_ERR_INVALID_ARG);
        assert(board_sdcard_file_read(&workspace, "0:x", "NOTE.TXT", trace, &diagnostic) ==
               ESP_ERR_INVALID_ARG);
        assert(board_sdcard_file_write(&workspace, "0:", "NOTE.TXT", "x\0y", 3, trace, &diagnostic) ==
               ESP_ERR_INVALID_ARG);
        assert(board_sdcard_file_write(&workspace, "0:", "NOTE.TXT", "x", BOARD_SDCARD_TEXT_MAX + 1, trace,
                                       &diagnostic) == ESP_ERR_INVALID_ARG);
        assert(used == 0);
    } else if (!strcmp(argv[1], "read") || !strcmp(argv[1], "read-error") || !strcmp(argv[1], "read-max") ||
               !strcmp(argv[1], "read-close-error") || !strcmp(argv[1], "read-both-errors") ||
               !strcmp(argv[1], "read-large") || !strcmp(argv[1], "read-binary")) {
        char data[BOARD_SDCARD_READ_MAX + 1];
        memset(data, 'x', sizeof(data));
        if (!strcmp(argv[1], "read-error") || !strcmp(argv[1], "read-both-errors"))
            fail_read = FR_DISK_ERR;
        if (!strcmp(argv[1], "read-close-error") || !strcmp(argv[1], "read-both-errors"))
            fail_close = FR_INT_ERR;
        if (!strcmp(argv[1], "read-max")) {
            read_data = data;
            read_size = BOARD_SDCARD_READ_MAX;
        }
        if (!strcmp(argv[1], "read-large")) {
            read_data = data;
            read_size = sizeof(data);
        }
        if (!strcmp(argv[1], "read-binary")) {
            read_data = "x\0y";
            read_size = 3;
        }
        error = board_sdcard_file_read(&workspace, "0:", "NOTE.TXT", trace, &diagnostic);
        assert(!strcmp(calls, "ORC"));
        assert(workspace.open == (fail_close != FR_OK) && workspace.quarantined == (fail_close != FR_OK));
        if (!strcmp(argv[1], "read") || !strcmp(argv[1], "read-max")) {
            assert(error == ESP_OK && workspace.length == read_size);
            assert(!memcmp(workspace.data, read_data, read_size) && workspace.data[read_size] == 0);
        } else {
            assert(error != ESP_OK && workspace.length == 0);
            assert(diagnostic.failed_phase == (fail_read    ? BOARD_SD_READ
                                               : fail_close ? BOARD_SD_CLOSE
                                                            : BOARD_SD_VALIDATE_READ));
            if (fail_read && fail_close) {
                assert(diagnostic.fat_result == FR_DISK_ERR && diagnostic.close_result == FR_INT_ERR);
            }
        }
    } else {
        if (!strcmp(argv[1], "open-error"))
            fail_open = FR_DENIED;
        else if (!strcmp(argv[1], "write-error"))
            fail_write = FR_DISK_ERR;
        else if (!strcmp(argv[1], "sync-error"))
            fail_sync = FR_DISK_ERR;
        else if (!strcmp(argv[1], "close-error"))
            fail_close = FR_DISK_ERR;
        else if (!strcmp(argv[1], "short-write"))
            short_write = true;
        else
            assert(!strcmp(argv[1], "write") || !strcmp(argv[1], "empty-write"));
        size_t size = !strcmp(argv[1], "empty-write") ? 0 : 4;
        error = board_sdcard_file_write(&workspace, "0:", "NOTE.TXT", "test", size, trace, &diagnostic);
        if (fail_open) {
            assert(error != ESP_OK && !strcmp(calls, "O") && !workspace.open);
            assert(diagnostic.failed_phase == BOARD_SD_OPEN_WRITE && diagnostic.fat_result == FR_DENIED);
        } else if (fail_write || fail_sync || fail_close) {
            assert(error != ESP_OK && workspace.quarantined && workspace.open);
            assert(!strcmp(calls, fail_write ? "OW" : fail_sync ? "OWS" : "OWSC"));
            assert(diagnostic.failed_phase == (fail_write  ? BOARD_SD_WRITE
                                               : fail_sync ? BOARD_SD_SYNC
                                                           : BOARD_SD_CLOSE));
            assert(diagnostic.fat_result == FR_DISK_ERR);
            /* No implicit f_close/f_sync retry after a dirty write/sync error; never reuse FIL. */
            char previous[32];
            memcpy(previous, calls, sizeof(previous));
            assert(board_sdcard_file_write(&workspace, "0:", "NOTE.TXT", "test", 4, trace, &diagnostic) ==
                   ESP_ERR_INVALID_STATE);
            assert(!strcmp(calls, previous));
        } else if (short_write) {
            assert(error != ESP_OK && !strcmp(calls, "OWC") && !workspace.open && !workspace.quarantined);
            assert(diagnostic.failed_phase == BOARD_SD_WRITE && diagnostic.fat_result == FR_OK);
        } else {
            assert(error == ESP_OK && !strcmp(calls, "OWSC") && !workspace.open && !workspace.quarantined);
            assert(diagnostic.failed_phase == BOARD_SD_IDLE);
        }
    }
    puts("PASS production file engine / real IDF 4KiB FIL layout; FatFs calls mocked, no device IO");
    return 0;
}
