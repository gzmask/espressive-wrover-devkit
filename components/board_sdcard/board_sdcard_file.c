#include "board_sdcard_file.h"
#include "esp_err.h"
#include <stdio.h>
#include <string.h>

_Static_assert(BOARD_SDCARD_MOUNT_PATH_MAX >= BOARD_SDCARD_FILENAME_MAX + 4,
               "Path includes digit, colon, slash and terminator");
static void report(board_sdcard_file_trace_t trace, void *user, board_sdcard_phase_t phase, int error,
                   int fat_result, size_t transferred, bool quarantined)
{
    if (trace)
        trace(user, phase, error, fat_result, transferred, quarantined);
}
static int prepare(board_sdcard_file_t *workspace, const char *drive, const char *name)
{
    if (!workspace || workspace->open || workspace->quarantined)
        return ESP_ERR_INVALID_STATE;
    workspace->length = 0;
    if (!drive || drive[0] < '0' || drive[0] > '9' || drive[1] != ':' || drive[2] != 0 || !name ||
        !board_sdcard_filename_valid(name, strlen(name)))
        return ESP_ERR_INVALID_ARG;
    int count = snprintf(workspace->path, sizeof(workspace->path), "%s/%s", drive, name);
    return count < 0 || (size_t)count >= sizeof(workspace->path) ? ESP_ERR_INVALID_SIZE : ESP_OK;
}
static int open_file(board_sdcard_file_t *workspace, BYTE mode, board_sdcard_phase_t phase,
                     board_sdcard_file_trace_t trace, void *user)
{
    report(trace, user, phase, ESP_OK, -1, 0, false);
    FRESULT result = f_open(&workspace->file, workspace->path, mode);
    int error = result == FR_OK ? ESP_OK : ESP_FAIL;
    if (error == ESP_OK)
        workspace->open = true;
    report(trace, user, phase, error, (int)result, 0, false);
    return error;
}
static int close_file(board_sdcard_file_t *workspace, size_t count, board_sdcard_file_trace_t trace,
                      void *user)
{
    report(trace, user, BOARD_SD_CLOSE, ESP_OK, -1, count, false);
    FRESULT result = f_close(&workspace->file);
    workspace->quarantined = result != FR_OK;
    if (result == FR_OK)
        workspace->open = false;
    int error = result == FR_OK ? ESP_OK : ESP_FAIL;
    report(trace, user, BOARD_SD_CLOSE, error, (int)result, count, workspace->quarantined);
    return error;
}
int board_sdcard_file_read(board_sdcard_file_t *workspace, const char *drive, const char *name,
                           board_sdcard_file_trace_t trace, void *user)
{
    int error = prepare(workspace, drive, name);
    if (error != ESP_OK) {
        report(trace, user, BOARD_SD_PATH, error, -1, 0, workspace && workspace->quarantined);
        return error;
    }
    error = open_file(workspace, FA_READ, BOARD_SD_OPEN_READ, trace, user);
    if (error != ESP_OK)
        return error;
    UINT count = 0;
    report(trace, user, BOARD_SD_READ, ESP_OK, -1, 0, false);
    FRESULT result = f_read(&workspace->file, workspace->data, sizeof(workspace->data), &count);
    error = result == FR_OK ? ESP_OK : ESP_FAIL;
    report(trace, user, BOARD_SD_READ, error, (int)result, count, false);
    int close_error = close_file(workspace, count, trace, user);
    if (error != ESP_OK || close_error != ESP_OK)
        return error != ESP_OK ? error : close_error;
    if (count > BOARD_SDCARD_READ_MAX || !board_sdcard_text_valid(workspace->data, count)) {
        report(trace, user, BOARD_SD_VALIDATE_READ, ESP_ERR_INVALID_SIZE, -1, count, false);
        return ESP_ERR_INVALID_SIZE;
    }
    workspace->data[count] = 0;
    workspace->length = count;
    return ESP_OK;
}
typedef struct {
    board_sdcard_file_t *workspace;
    board_sdcard_source_cancel_fn cancelled;
    void *cancel_user;
    FRESULT fat_result;
} source_reader_t;
static int source_read_chunk(void *user, char *buffer, size_t capacity, size_t *count)
{
    source_reader_t *reader = user;
    UINT transferred = 0;
    reader->fat_result = f_read(&reader->workspace->file, buffer, (UINT)capacity, &transferred);
    *count = transferred;
    return reader->fat_result == FR_OK ? 0 : -1;
}
static bool source_cancelled(void *user)
{
    source_reader_t *reader = user;
    return reader->cancelled(reader->cancel_user);
}
int board_sdcard_file_read_source(board_sdcard_file_t *workspace, const char *drive, const char *name,
                                  char *source, size_t capacity, size_t *length,
                                  board_sdcard_source_cancel_fn cancelled, void *cancel_user,
                                  board_sdcard_file_trace_t trace, void *trace_user)
{
    if (!length)
        return ESP_ERR_INVALID_ARG;
    *length = 0;
    if (!source || capacity < BOARD_SDCARD_SOURCE_LIMIT + 1 || !cancelled)
        return ESP_ERR_INVALID_ARG;
    int error = prepare(workspace, drive, name);
    if (error != ESP_OK) {
        report(trace, trace_user, BOARD_SD_PATH, error, -1, 0, workspace && workspace->quarantined);
        return error;
    }
    error = open_file(workspace, FA_READ, BOARD_SD_OPEN_READ, trace, trace_user);
    if (error != ESP_OK)
        return error;
    source_reader_t reader = {.workspace = workspace, .cancelled = cancelled, .cancel_user = cancel_user};
    report(trace, trace_user, BOARD_SD_READ, ESP_OK, -1, 0, false);
    bool success =
        board_sdcard_source_read(source_read_chunk, source_cancelled, &reader, source, capacity, length);
    error = success ? ESP_OK : ESP_FAIL;
    report(trace, trace_user, BOARD_SD_READ, error, (int)reader.fat_result, *length, false);
    int close_error = close_file(workspace, *length, trace, trace_user);
    if (close_error != ESP_OK) {
        *length = 0;
        source[0] = 0;
        return close_error;
    }
    return error;
}
int board_sdcard_file_write(board_sdcard_file_t *workspace, const char *drive, const char *name,
                            const char *text, size_t length, board_sdcard_file_trace_t trace, void *user)
{
    int error = prepare(workspace, drive, name);
    if (error == ESP_OK && !board_sdcard_text_valid(text, length))
        error = ESP_ERR_INVALID_ARG;
    if (error != ESP_OK) {
        report(trace, user, BOARD_SD_PATH, error, -1, 0, workspace && workspace->quarantined);
        return error;
    }
    error = open_file(workspace, FA_WRITE | FA_CREATE_ALWAYS, BOARD_SD_OPEN_WRITE, trace, user);
    if (error != ESP_OK)
        return error;
    UINT count = 0;
    report(trace, user, BOARD_SD_WRITE, ESP_OK, -1, 0, false);
    FRESULT result = f_write(&workspace->file, text, (UINT)length, &count);
    error = result == FR_OK && count == length ? ESP_OK : ESP_FAIL;
    /* f_close would call f_sync again. Never implicitly retry a failed dirty write. */
    workspace->quarantined = result != FR_OK;
    report(trace, user, BOARD_SD_WRITE, error, (int)result, count, workspace->quarantined);
    if (workspace->quarantined)
        return error;
    if (error == ESP_OK) {
        report(trace, user, BOARD_SD_SYNC, ESP_OK, -1, count, false);
        result = f_sync(&workspace->file);
        error = result == FR_OK ? ESP_OK : ESP_FAIL;
        workspace->quarantined = result != FR_OK;
        report(trace, user, BOARD_SD_SYNC, error, (int)result, count, workspace->quarantined);
        if (workspace->quarantined)
            return error;
    }
    int close_error = close_file(workspace, count, trace, user);
    return error != ESP_OK ? error : close_error;
}
