/* Production file engine + pinned real FatFs. All sectors are anonymous heap
 * memory, never a host file/block device or a physical SD card. */
#include "board_sdcard_file.h"
#include "diskio.h"
#include "esp_err.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SECTOR_BYTES = 512, SECTOR_COUNT = 131072 };
static BYTE *image;
static bool fail_sync;
static unsigned reads, writes, syncs;
static bool locked[FF_VOLUMES + 1];
static FATFS filesystem;
static BYTE format_work[4096];
static board_sdcard_file_t workspace;
static board_sdcard_diagnostic_t diagnostic;
static char loaded_source[BOARD_SDCARD_SOURCE_LIMIT + 1];
static char large_source[BOARD_SDCARD_SOURCE_LIMIT + 1];
static FIL fixture_file;
static bool never_cancel(void *user)
{
    (void)user;
    return false;
}
PARTITION VolToPart[FF_VOLUMES] = {{0, 0}, {1, 0}};
DSTATUS ff_disk_initialize(BYTE drive)
{
    return drive == 0 && image ? 0 : STA_NOINIT;
}
DSTATUS ff_disk_status(BYTE drive)
{
    return ff_disk_initialize(drive);
}
DRESULT ff_disk_read(BYTE drive, BYTE *data, LBA_t sector, UINT count)
{
    if (drive != 0 || !image || sector >= SECTOR_COUNT || count > SECTOR_COUNT - sector)
        return RES_PARERR;
    reads++;
    memcpy(data, image + (size_t)sector * SECTOR_BYTES, (size_t)count * SECTOR_BYTES);
    return RES_OK;
}
DRESULT ff_disk_write(BYTE drive, const BYTE *data, LBA_t sector, UINT count)
{
    if (drive != 0 || !image || sector >= SECTOR_COUNT || count > SECTOR_COUNT - sector)
        return RES_PARERR;
    writes++;
    memcpy(image + (size_t)sector * SECTOR_BYTES, data, (size_t)count * SECTOR_BYTES);
    return RES_OK;
}
DRESULT ff_disk_ioctl(BYTE drive, BYTE command, void *data)
{
    if (drive != 0 || !image)
        return RES_PARERR;
    switch (command) {
    case CTRL_SYNC:
        syncs++;
        return fail_sync ? RES_ERROR : RES_OK;
    case GET_SECTOR_COUNT:
        *(LBA_t *)data = SECTOR_COUNT;
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD *)data = SECTOR_BYTES;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)data = 1;
        return RES_OK;
    case CTRL_TRIM:
        return RES_OK;
    default:
        return RES_PARERR;
    }
}
DWORD get_fattime(void)
{
    return ((DWORD)46 << 25) | ((DWORD)11 << 21) | ((DWORD)14 << 16);
}
void *ff_memalloc(UINT size)
{
    return malloc(size);
}
void ff_memfree(void *data)
{
    free(data);
}
int ff_mutex_create(int volume)
{
    assert(volume >= 0 && volume <= FF_VOLUMES);
    return 1;
}
void ff_mutex_delete(int volume)
{
    assert(!locked[volume]);
}
int ff_mutex_take(int volume)
{
    assert(!locked[volume]);
    locked[volume] = true;
    return 1;
}
void ff_mutex_give(int volume)
{
    assert(locked[volume]);
    locked[volume] = false;
}
static void trace(void *user, board_sdcard_phase_t phase, int error, int fat, size_t count, bool quarantined)
{
    assert(user == &diagnostic);
    board_sdcard_diagnostic_record(user, phase, error, fat, UINT32_MAX, count, quarantined);
}
int main(int argc, char **argv)
{
    assert(argc == 2 && sizeof(FIL) >= 4096);
    image = calloc(SECTOR_COUNT, SECTOR_BYTES);
    assert(image != NULL);
    /* Test setup only. Firmware's file engine never calls f_mkfs. */
    const MKFS_PARM parameters = {.fmt = FM_FAT32, .n_fat = 2, .au_size = SECTOR_BYTES};
    assert(f_mkfs("0:", &parameters, format_work, sizeof(format_work)) == FR_OK);
    assert(f_mount(&filesystem, "0:", 1) == FR_OK && filesystem.fs_type == FS_FAT32);
    const char *payload = "WROVER 0.5.10 SD acceptance 7D9C21A\nASCII roundtrip: 0123456789\n";
    board_sdcard_diagnostic_begin(&diagnostic, strlen(payload));
    if (!strcmp(argv[1], "missing")) {
        unsigned before = writes;
        assert(board_sdcard_file_read(&workspace, "0:", "ABSENT.TXT", trace, &diagnostic) == ESP_FAIL);
        assert(diagnostic.failed_phase == BOARD_SD_OPEN_READ && diagnostic.fat_result == FR_NO_FILE);
        assert(writes == before && !workspace.open && !workspace.quarantined);
    } else if (!strcmp(argv[1], "sync-error")) {
        fail_sync = true;
        assert(board_sdcard_file_write(&workspace, "0:", "NOTE.TXT", payload, strlen(payload), trace,
                                       &diagnostic) == ESP_FAIL);
        assert(diagnostic.failed_phase == BOARD_SD_SYNC && diagnostic.fat_result == FR_DISK_ERR);
        assert(diagnostic.close_result == -1 && workspace.quarantined && workspace.open);
        unsigned before = writes, previous_syncs = syncs;
        assert(board_sdcard_file_write(&workspace, "0:", "NOTE.TXT", payload, strlen(payload), trace,
                                       &diagnostic) == ESP_ERR_INVALID_STATE);
        assert(writes == before && syncs == previous_syncs);
    } else if (!strcmp(argv[1], "source-bounds")) {
        memset(large_source, 'x', sizeof(large_source));
        UINT written = 0;
        assert(f_open(&fixture_file, "0:/APP.LSP", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK);
        assert(f_write(&fixture_file, large_source, BOARD_SDCARD_SOURCE_LIMIT, &written) == FR_OK);
        assert(written == BOARD_SDCARD_SOURCE_LIMIT && f_close(&fixture_file) == FR_OK);
        size_t length = 0;
        assert(board_sdcard_file_read_source(&workspace, "0:", "APP.LSP", loaded_source,
                                             sizeof(loaded_source), &length, never_cancel, NULL, trace,
                                             &diagnostic) == ESP_OK);
        assert(length == BOARD_SDCARD_SOURCE_LIMIT && loaded_source[length] == 0);
        assert(!memcmp(loaded_source, large_source, length));
        assert(f_open(&fixture_file, "0:/APP.LSP", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK);
        assert(f_write(&fixture_file, large_source, sizeof(large_source), &written) == FR_OK);
        assert(written == sizeof(large_source) && f_close(&fixture_file) == FR_OK);
        assert(board_sdcard_file_read_source(&workspace, "0:", "APP.LSP", loaded_source,
                                             sizeof(loaded_source), &length, never_cancel, NULL, trace,
                                             &diagnostic) == ESP_FAIL);
        assert(length == 0 && loaded_source[0] == 0 && !workspace.open && !workspace.quarantined);
    } else {
        assert(!strcmp(argv[1], "roundtrip"));
        for (unsigned iteration = 0; iteration < 20; iteration++) {
            board_sdcard_diagnostic_begin(&diagnostic, strlen(payload));
            assert(board_sdcard_file_write(&workspace, "0:", "NOTE.TXT", payload, strlen(payload), trace,
                                           &diagnostic) == ESP_OK);
            assert(!workspace.open && !workspace.quarantined);
            assert(f_mount(NULL, "0:", 0) == FR_OK);
            assert(f_mount(&filesystem, "0:", 1) == FR_OK);
            board_sdcard_diagnostic_begin(&diagnostic, BOARD_SDCARD_READ_MAX + 1);
            assert(board_sdcard_file_read(&workspace, "0:", "NOTE.TXT", trace, &diagnostic) == ESP_OK);
            assert(workspace.length == strlen(payload) && !strcmp(workspace.data, payload));
            size_t source_length = 0;
            assert(board_sdcard_file_read_source(&workspace, "0:", "NOTE.TXT", loaded_source,
                                                 sizeof(loaded_source), &source_length, never_cancel, NULL,
                                                 trace, &diagnostic) == ESP_OK);
            assert(source_length == strlen(payload) && !strcmp(loaded_source, payload));
        }
        char maximum[BOARD_SDCARD_TEXT_MAX];
        memset(maximum, 'x', sizeof(maximum));
        assert(board_sdcard_file_write(&workspace, "0:", "NOTE.TXT", maximum, sizeof(maximum), trace,
                                       &diagnostic) == ESP_OK);
        assert(board_sdcard_file_read(&workspace, "0:", "NOTE.TXT", trace, &diagnostic) ==
               ESP_ERR_INVALID_SIZE);
        assert(!workspace.open && !workspace.quarantined && workspace.length == 0);
        assert(board_sdcard_file_write(&workspace, "0:", "NOTE.TXT", "", 0, trace, &diagnostic) == ESP_OK);
        assert(board_sdcard_file_read(&workspace, "0:", "NOTE.TXT", trace, &diagnostic) == ESP_OK);
        assert(workspace.length == 0 && workspace.data[0] == 0 && reads && writes && syncs);
    }
    assert(f_mount(NULL, "0:", 0) == FR_OK);
    free(image);
    puts("PASS real pinned FatFs/FAT32 on heap-only sectors, 4KiB FIL; not FreeRTOS/SDSPI/device evidence");
    return 0;
}
