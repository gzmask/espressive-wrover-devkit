#include "board_sdcard.h"
#include "board_display.h"
#include "board_sdcard_file.h"
#include <inttypes.h>
#include <string.h>

#include "diskio_impl.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "diskio_sdmmc.h"
#include "ff.h"
#include "sdmmc_cmd.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

enum {
    SDCARD_MISO = 2,
    SDCARD_CS = 13,
    SDCARD_SCLK = 14,
    SDCARD_MOSI = 15,
    SDCARD_DETECT = 21,
    SDCARD_WORKER_STACK_BYTES = 6144,
    SDCARD_QUEUE_LENGTH = 1,
    SDCARD_SPI_MAX_TRANSFER = 4096,
};
static const char *TAG = "board_sdcard";
typedef enum { REQUEST_MOUNT, REQUEST_UNMOUNT, REQUEST_READ, REQUEST_WRITE, REQUEST_SOURCE } request_kind_t;
typedef struct {
    request_kind_t kind;
    size_t name_length;
    size_t text_length;
    char name[BOARD_SDCARD_FILENAME_MAX + 1];
    char text[BOARD_SDCARD_TEXT_MAX + 1];
} request_t;
static QueueHandle_t requests;
static TaskHandle_t worker;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static board_sdcard_state_t state = BOARD_SDCARD_DISABLED;
static esp_err_t last_error = ESP_OK;
static bool started;
static bool display_lease;
static bool spi_bus_ready;
static bool sdspi_device_ready;
static bool disk_registered;
static bool fat_mounted;
static int sdspi_device = -1;
static BYTE drive = FF_DRV_NOT_USED;
static char drive_path[3];
static sdmmc_host_t active_host;
static sdmmc_card_t *card;
static FATFS filesystem;
static char read_result[BOARD_SDCARD_READ_MAX + 1];
static size_t read_result_length;
static bool read_result_consuming;
/* Single-worker ownership: no configuration-sized FIL/request on task stacks. */
static board_sdcard_file_t file_workspace;
static request_t worker_request;
static board_sdcard_source_slot_t source_slot;
static board_sdcard_diagnostic_t diagnostic = {
    .fat_result = -1, .close_result = -1, .stack_min_free = UINT32_MAX};

void board_sdcard_get_diagnostics(board_sdcard_diagnostic_t *destination)
{
    if (destination == NULL)
        return;
    portENTER_CRITICAL(&state_lock);
    *destination = diagnostic;
    portEXIT_CRITICAL(&state_lock);
}
/* Called only by the SD worker; ESP-IDF's high-water API reports bytes, not words.
 * Observe before/after SDK calls; this cannot make an overflowing SDK call safe. */
static void trace_phase(void *user, board_sdcard_phase_t phase, int error, int fat_result, size_t transferred,
                        bool quarantined)
{
    (void)user;
    uint32_t stack_free = (uint32_t)uxTaskGetStackHighWaterMark(NULL);
    portENTER_CRITICAL(&state_lock);
    board_sdcard_diagnostic_record(&diagnostic, phase, error, fat_result, stack_free, transferred,
                                   quarantined);
    uint32_t sequence = diagnostic.sequence;
    uint32_t minimum = diagnostic.stack_min_free;
    portEXIT_CRITICAL(&state_lock);
    ESP_LOGI(TAG, "seq=%" PRIu32 " phase=%s esp=%d fat=%d bytes=%u stack_min=%" PRIu32 " quarantine=%u",
             sequence, board_sdcard_phase_name(phase), error, fat_result, (unsigned)transferred, minimum,
             quarantined ? 1u : 0u);
}
static void fail_current_phase(esp_err_t error)
{
    portENTER_CRITICAL(&state_lock);
    board_sdcard_phase_t phase = diagnostic.phase;
    portEXIT_CRITICAL(&state_lock);
    trace_phase(NULL, phase, error, -1, 0, false);
}

static void publish(board_sdcard_state_t next, esp_err_t error)
{
    portENTER_CRITICAL(&state_lock);
    state = next;
    last_error = error;
    portEXIT_CRITICAL(&state_lock);
}
board_sdcard_state_t board_sdcard_state(void)
{
    portENTER_CRITICAL(&state_lock);
    board_sdcard_state_t value = state;
    portEXIT_CRITICAL(&state_lock);
    return value;
}
int board_sdcard_last_error(void)
{
    portENTER_CRITICAL(&state_lock);
    int value = (int)last_error;
    portEXIT_CRITICAL(&state_lock);
    return value;
}
static bool send_request(const request_t *request, board_sdcard_state_t expected,
                         board_sdcard_state_t pending)
{
    portENTER_CRITICAL(&state_lock);
    if (state != expected || requests == NULL) {
        portEXIT_CRITICAL(&state_lock);
        return false;
    }
    state = pending;
    last_error = ESP_OK;
    board_sdcard_diagnostic_begin(&diagnostic, request->kind == REQUEST_READ ? BOARD_SDCARD_READ_MAX + 1
                                                                             : request->text_length);
    portEXIT_CRITICAL(&state_lock);
    if (xQueueSend(requests, request, 0) == pdTRUE)
        return true;
    portENTER_CRITICAL(&state_lock);
    board_sdcard_diagnostic_record(&diagnostic, BOARD_SD_QUEUED, ESP_ERR_TIMEOUT, -1, UINT32_MAX, 0, false);
    portEXIT_CRITICAL(&state_lock);
    publish(expected, ESP_ERR_TIMEOUT);
    return false;
}
static bool release_display_lease(void)
{
    portENTER_CRITICAL(&state_lock);
    bool held = display_lease;
    portEXIT_CRITICAL(&state_lock);
    if (!held)
        return true;
    if (!board_display_sd_release())
        return false;
    portENTER_CRITICAL(&state_lock);
    display_lease = false;
    portEXIT_CRITICAL(&state_lock);
    return true;
}
bool board_sdcard_request_mount(void)
{
    board_sdcard_state_t current = board_sdcard_state();
    display_lifecycle_state_t display = board_display_status();
    if (!started || !board_sdcard_can_mount(current, display, true) || !board_display_sd_acquire())
        return false;
    portENTER_CRITICAL(&state_lock);
    display_lease = true;
    portEXIT_CRITICAL(&state_lock);
    request_t request = {.kind = REQUEST_MOUNT};
    if (send_request(&request, BOARD_SDCARD_UNMOUNTED, BOARD_SDCARD_MOUNTING))
        return true;
    if (!release_display_lease())
        publish(BOARD_SDCARD_FAULT, ESP_ERR_INVALID_STATE);
    return false;
}
bool board_sdcard_request_unmount(void)
{
    request_t request = {.kind = REQUEST_UNMOUNT};
    return started && board_sdcard_can_unmount(board_sdcard_state()) &&
           send_request(&request, BOARD_SDCARD_MOUNTED, BOARD_SDCARD_UNMOUNTING);
}
bool board_sdcard_request_read(const char *name, size_t length)
{
    if (!started || !board_sdcard_can_file_io(board_sdcard_state()) ||
        !board_sdcard_filename_valid(name, length))
        return false;
    request_t request = {.kind = REQUEST_READ, .name_length = length};
    memcpy(request.name, name, length);
    request.name[length] = 0;
    return send_request(&request, BOARD_SDCARD_MOUNTED, BOARD_SDCARD_READING);
}
bool board_sdcard_request_write(const char *name, size_t name_length, const char *text, size_t text_length)
{
    if (!started || !board_sdcard_can_file_io(board_sdcard_state()) ||
        !board_sdcard_filename_valid(name, name_length) || !board_sdcard_text_valid(text, text_length))
        return false;
    request_t request = {.kind = REQUEST_WRITE, .name_length = name_length, .text_length = text_length};
    memcpy(request.name, name, name_length);
    request.name[name_length] = 0;
    if (text_length > 0)
        memcpy(request.text, text, text_length);
    request.text[text_length] = 0;
    return send_request(&request, BOARD_SDCARD_MOUNTED, BOARD_SDCARD_WRITING);
}
bool board_sdcard_source_request(const char *name, size_t length)
{
    if (!started || !board_sdcard_filename_valid(name, length))
        return false;
    request_t request = {.kind = REQUEST_SOURCE, .name_length = length};
    memcpy(request.name, name, length);
    request.name[length] = 0;
    portENTER_CRITICAL(&state_lock);
    if (state != BOARD_SDCARD_MOUNTED || requests == NULL || !board_sdcard_source_slot_begin(&source_slot)) {
        portEXIT_CRITICAL(&state_lock);
        return false;
    }
    state = BOARD_SDCARD_READING;
    last_error = ESP_OK;
    board_sdcard_diagnostic_begin(&diagnostic, BOARD_SDCARD_SOURCE_LIMIT + 1);
    portEXIT_CRITICAL(&state_lock);
    if (xQueueSend(requests, &request, 0) == pdTRUE)
        return true;
    portENTER_CRITICAL(&state_lock);
    board_sdcard_source_slot_finish(&source_slot, false, 0);
    source_slot.phase = BOARD_SOURCE_IDLE;
    state = BOARD_SDCARD_MOUNTED;
    last_error = ESP_ERR_TIMEOUT;
    board_sdcard_diagnostic_record(&diagnostic, BOARD_SD_QUEUED, ESP_ERR_TIMEOUT, -1, UINT32_MAX, 0, false);
    portEXIT_CRITICAL(&state_lock);
    return false;
}
board_sdcard_source_phase_t board_sdcard_source_poll(const char **source, size_t *length)
{
    if (!source || !length)
        return BOARD_SOURCE_FAILED;
    portENTER_CRITICAL(&state_lock);
    board_sdcard_source_phase_t result = board_sdcard_source_slot_poll(&source_slot, source, length);
    if (result == BOARD_SOURCE_FAILED && state != BOARD_SDCARD_FAULT)
        state = BOARD_SDCARD_MOUNTED;
    portEXIT_CRITICAL(&state_lock);
    return result;
}
void board_sdcard_source_cancel(void)
{
    portENTER_CRITICAL(&state_lock);
    board_sdcard_source_phase_t previous = source_slot.phase;
    board_sdcard_source_slot_cancel(&source_slot);
    if ((previous == BOARD_SOURCE_READY || previous == BOARD_SOURCE_FAILED) && state != BOARD_SDCARD_FAULT)
        state = BOARD_SDCARD_MOUNTED;
    portEXIT_CRITICAL(&state_lock);
}
void board_sdcard_source_release(void)
{
    portENTER_CRITICAL(&state_lock);
    if (source_slot.phase == BOARD_SOURCE_LEASED) {
        board_sdcard_source_slot_release(&source_slot);
        if (state != BOARD_SDCARD_FAULT)
            state = BOARD_SDCARD_MOUNTED;
    }
    portEXIT_CRITICAL(&state_lock);
}
size_t board_sdcard_read_result_size(void)
{
    portENTER_CRITICAL(&state_lock);
    size_t length = state == BOARD_SDCARD_READ_READY ? read_result_length : 0;
    portEXIT_CRITICAL(&state_lock);
    return length;
}
bool board_sdcard_take_read_result(char *destination, size_t capacity, size_t *length)
{
    if (destination == NULL || length == NULL)
        return false;
    portENTER_CRITICAL(&state_lock);
    if (state != BOARD_SDCARD_READ_READY || read_result_consuming || capacity <= read_result_length) {
        portEXIT_CRITICAL(&state_lock);
        return false;
    }
    read_result_consuming = true;
    size_t available = read_result_length;
    portEXIT_CRITICAL(&state_lock);
    memcpy(destination, read_result, available + 1);
    portENTER_CRITICAL(&state_lock);
    if (state != BOARD_SDCARD_READ_READY || read_result_length != available) {
        read_result_consuming = false;
        portEXIT_CRITICAL(&state_lock);
        return false;
    }
    *length = available;
    read_result_length = 0;
    read_result_consuming = false;
    state = BOARD_SDCARD_MOUNTED;
    portEXIT_CRITICAL(&state_lock);
    return true;
}
bool board_sdcard_take_operation_result(board_sdcard_state_t *completed_state)
{
    if (completed_state == NULL)
        return false;
    portENTER_CRITICAL(&state_lock);
    if (state != BOARD_SDCARD_WRITE_DONE && state != BOARD_SDCARD_OPERATION_ERROR) {
        portEXIT_CRITICAL(&state_lock);
        return false;
    }
    *completed_state = state;
    state = BOARD_SDCARD_MOUNTED;
    portEXIT_CRITICAL(&state_lock);
    return true;
}
static bool build_drive_path(void)
{
    if (drive >= FF_VOLUMES || (unsigned)('0' + drive) > 255)
        return false;
    drive_path[0] = (char)('0' + drive);
    drive_path[1] = ':';
    drive_path[2] = 0;
    return true;
}
static esp_err_t release_mount_resources(bool unmount_fat)
{
    if (unmount_fat && disk_registered && fat_mounted) {
        trace_phase(NULL, BOARD_SD_FAT_UNMOUNT, ESP_OK, -1, 0, false);
        FRESULT result = f_mount(NULL, drive_path, 0);
        trace_phase(NULL, BOARD_SD_FAT_UNMOUNT, result == FR_OK ? ESP_OK : ESP_FAIL, (int)result, 0,
                    result != FR_OK);
        if (result != FR_OK)
            return ESP_FAIL;
        fat_mounted = false;
    }
    if (disk_registered) {
        ff_diskio_unregister(drive);
        disk_registered = false;
        drive = FF_DRV_NOT_USED;
    }
    if (sdspi_device_ready) {
        trace_phase(NULL, BOARD_SD_DEVICE_REMOVE, ESP_OK, -1, 0, false);
        esp_err_t error = sdspi_host_remove_device(sdspi_device);
        trace_phase(NULL, BOARD_SD_DEVICE_REMOVE, error, -1, 0, error != ESP_OK);
        if (error != ESP_OK)
            return error;
        sdspi_device_ready = false;
        sdspi_device = -1;
    }
    free(card);
    card = NULL;
    if (spi_bus_ready) {
        trace_phase(NULL, BOARD_SD_BUS_FREE, ESP_OK, -1, 0, false);
        esp_err_t error = spi_bus_free(SDSPI_DEFAULT_HOST);
        trace_phase(NULL, BOARD_SD_BUS_FREE, error, -1, 0, error != ESP_OK);
        if (error != ESP_OK)
            return error;
        spi_bus_ready = false;
    }
    return ESP_OK;
}
static esp_err_t mount_card(void)
{
    const spi_bus_config_t bus = {
        .mosi_io_num = SDCARD_MOSI,
        .miso_io_num = SDCARD_MISO,
        .sclk_io_num = SDCARD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SDCARD_SPI_MAX_TRANSFER,
    };
    trace_phase(NULL, BOARD_SD_SPI_BUS_INIT, ESP_OK, -1, 0, false);
    esp_err_t error = spi_bus_initialize(SDSPI_DEFAULT_HOST, &bus, SDSPI_DEFAULT_DMA);
    if (error != ESP_OK)
        goto mount_failed;
    spi_bus_ready = true;
    active_host = (sdmmc_host_t)SDSPI_HOST_DEFAULT();
    active_host.max_freq_khz = 10000;
    active_host.unaligned_multi_block_rw_max_chunk_size = 8;
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.host_id = active_host.slot;
    slot.gpio_cs = SDCARD_CS;
    slot.gpio_cd = SDCARD_DETECT;
    trace_phase(NULL, BOARD_SD_HOST_INIT, ESP_OK, -1, 0, false);
    error = active_host.init();
    if (error != ESP_OK)
        goto mount_failed;
    trace_phase(NULL, BOARD_SD_DEVICE_INIT, ESP_OK, -1, 0, false);
    error = sdspi_host_init_device(&slot, &sdspi_device);
    if (error != ESP_OK)
        goto mount_failed;
    sdspi_device_ready = true;
    active_host.slot = sdspi_device;
    trace_phase(NULL, BOARD_SD_CARD_ALLOC, ESP_OK, -1, 0, false);
    card = calloc(1, sizeof(*card));
    if (card == NULL) {
        error = ESP_ERR_NO_MEM;
        goto mount_failed;
    }
    trace_phase(NULL, BOARD_SD_CARD_INIT, ESP_OK, -1, 0, false);
    error = sdmmc_card_init(&active_host, card);
    if (error != ESP_OK)
        goto mount_failed;
    trace_phase(NULL, BOARD_SD_DRIVE_ALLOC, ESP_OK, -1, 0, false);
    error = ff_diskio_get_drive(&drive);
    if (error != ESP_OK)
        goto mount_failed;
    trace_phase(NULL, BOARD_SD_PATH, ESP_OK, -1, 0, false);
    if (!build_drive_path()) {
        error = ESP_ERR_INVALID_STATE;
        goto mount_failed;
    }
    ff_diskio_register_sdmmc(drive, card);
    disk_registered = true;
    ff_sdmmc_set_disk_status_check(drive, true);
    fat_mounted =
        true; /* On mount error, detach any partially registered FAT volume before releasing pins. */
    trace_phase(NULL, BOARD_SD_FAT_MOUNT, ESP_OK, -1, 0, false);
    FRESULT fat_error = f_mount(&filesystem, drive_path, 1);
    trace_phase(NULL, BOARD_SD_FAT_MOUNT, fat_error == FR_OK ? ESP_OK : ESP_FAIL, (int)fat_error, 0, false);
    if (fat_error != FR_OK) {
        error = ESP_FAIL;
        ESP_LOGE(TAG, "FAT mount failed (FRESULT=%d); card will not be formatted", (int)fat_error);
        goto mount_failed;
    }
    trace_phase(NULL, BOARD_SD_COMPLETE, ESP_OK, -1, 0, false);
    publish(board_sdcard_mount_complete(true), ESP_OK);
    ESP_LOGI(TAG, "FAT SD mounted via SDSPI at %u kHz; auto-format disabled",
             (unsigned)active_host.max_freq_khz);
    return ESP_OK;

mount_failed:
    {
        fail_current_phase(error);
        esp_err_t cleanup = release_mount_resources(true);
        if (cleanup == ESP_OK) {
            trace_phase(NULL, BOARD_SD_LEASE_RELEASE, ESP_OK, -1, 0, false);
            if (!release_display_lease()) {
                cleanup = ESP_ERR_INVALID_STATE;
                trace_phase(NULL, BOARD_SD_LEASE_RELEASE, cleanup, -1, 0, true);
            }
        }
        if (cleanup != ESP_OK) {
            publish(BOARD_SDCARD_FAULT, cleanup);
            return error;
        }
        publish(board_sdcard_mount_complete(false), error);
        return error;
    }
}
static esp_err_t unmount_card(void)
{
    esp_err_t error = release_mount_resources(true);
    if (error != ESP_OK) {
        publish(BOARD_SDCARD_FAULT, error);
        return error; /* Do not release LCD/SD pins when storage cleanup is uncertain. */
    }
    trace_phase(NULL, BOARD_SD_LEASE_RELEASE, ESP_OK, -1, 0, false);
    if (!release_display_lease()) {
        trace_phase(NULL, BOARD_SD_LEASE_RELEASE, ESP_ERR_INVALID_STATE, -1, 0, true);
        publish(BOARD_SDCARD_FAULT, ESP_ERR_INVALID_STATE);
        return ESP_ERR_INVALID_STATE;
    }
    trace_phase(NULL, BOARD_SD_COMPLETE, ESP_OK, -1, 0, false);
    publish(board_sdcard_unmount_complete(true), ESP_OK);
    return ESP_OK;
}
static esp_err_t read_file(const request_t *request)
{
    esp_err_t error = board_sdcard_file_read(&file_workspace, drive_path, request->name, trace_phase, NULL);
    if (error != ESP_OK)
        return error;
    memcpy(read_result, file_workspace.data, file_workspace.length + 1);
    portENTER_CRITICAL(&state_lock);
    read_result_length = file_workspace.length;
    portEXIT_CRITICAL(&state_lock);
    /* Publish READ_READY only once, in finish_io, after all copies are complete. */
    return ESP_OK;
}
static esp_err_t write_file(const request_t *request)
{
    return board_sdcard_file_write(&file_workspace, drive_path, request->name, request->text,
                                   request->text_length, trace_phase, NULL);
}
static void finish_io(bool reading, esp_err_t error)
{
    if (error == ESP_OK)
        trace_phase(NULL, BOARD_SD_COMPLETE, ESP_OK, -1,
                    reading ? file_workspace.length : worker_request.text_length, false);
    portENTER_CRITICAL(&state_lock);
    state = board_sdcard_io_complete(reading, error == ESP_OK, file_workspace.quarantined);
    last_error = error;
    portEXIT_CRITICAL(&state_lock);
}

static bool source_cancelled(void *user)
{
    (void)user;
    portENTER_CRITICAL(&state_lock);
    bool cancelled = source_slot.cancelled;
    portEXIT_CRITICAL(&state_lock);
    return cancelled;
}
static void read_source(const request_t *request)
{
    size_t length = 0;
    esp_err_t error = board_sdcard_file_read_source(&file_workspace, drive_path, request->name,
                                                    source_slot.buffer, source_slot.capacity, &length,
                                                    source_cancelled, NULL, trace_phase, NULL);
    portENTER_CRITICAL(&state_lock);
    board_sdcard_source_slot_finish(&source_slot, error == ESP_OK && !file_workspace.quarantined, length);
    last_error = error;
    if (file_workspace.quarantined)
        state = BOARD_SDCARD_FAULT;
    else if (source_slot.phase == BOARD_SOURCE_IDLE)
        state = BOARD_SDCARD_MOUNTED; /* cancelled worker has finally finished */
    portEXIT_CRITICAL(&state_lock);
}
static void worker_task(void *argument)
{
    (void)argument;
    ESP_LOGI(TAG, "worker stack=%u workspace=%u FIL=%u; low-water samples are not a hard stack bound",
             SDCARD_WORKER_STACK_BYTES, (unsigned)sizeof(file_workspace), (unsigned)sizeof(FIL));
    for (;;) {
        if (xQueueReceive(requests, &worker_request, portMAX_DELAY) != pdTRUE)
            continue;
        switch (worker_request.kind) {
        case REQUEST_MOUNT:
            (void)mount_card();
            break;
        case REQUEST_UNMOUNT:
            (void)unmount_card();
            break;
        case REQUEST_READ:
            finish_io(true, read_file(&worker_request));
            break;
        case REQUEST_WRITE:
            finish_io(false, write_file(&worker_request));
            break;
        case REQUEST_SOURCE:
            read_source(&worker_request);
            break;
        }
    }
}
esp_err_t board_sdcard_start(void)
{
    if (started)
        return ESP_ERR_INVALID_STATE;
    /* Optional loader buffer: PSRAM only; ordinary SD remains usable if allocation fails. */
    char *source = heap_caps_calloc(1, BOARD_SDCARD_SOURCE_LIMIT + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (source && !board_sdcard_source_slot_init(&source_slot, source, BOARD_SDCARD_SOURCE_LIMIT + 1)) {
        heap_caps_free(source);
        source = NULL;
    }
    if (!source)
        ESP_LOGE(TAG, "SD app loader PSRAM unavailable; ordinary SD API remains available");
    requests = xQueueCreate(SDCARD_QUEUE_LENGTH, sizeof(request_t));
    if (requests == NULL) {
        heap_caps_free(source);
        source_slot = (board_sdcard_source_slot_t){0};
        publish(BOARD_SDCARD_FAULT, ESP_ERR_NO_MEM);
        return ESP_ERR_NO_MEM;
    }
    publish(BOARD_SDCARD_UNMOUNTED, ESP_OK);
    if (xTaskCreate(worker_task, "sd_card", SDCARD_WORKER_STACK_BYTES, NULL, 1, &worker) != pdPASS) {
        vQueueDelete(requests);
        requests = NULL;
        heap_caps_free(source);
        source_slot = (board_sdcard_source_slot_t){0};
        publish(BOARD_SDCARD_FAULT, ESP_ERR_NO_MEM);
        return ESP_ERR_NO_MEM;
    }
    started = true;
    return ESP_OK;
}
