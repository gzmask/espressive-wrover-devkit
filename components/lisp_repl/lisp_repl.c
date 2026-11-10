#include "lisp_repl.h"
#include "sdkconfig.h"

#if CONFIG_LISP_REPL_ENABLE
#if CONFIG_ESP_CONSOLE_UART_NUM != 0 || CONFIG_ESP_CONSOLE_UART_BAUDRATE != 115200
#error "The first REPL profile requires the existing UART0 console at 115200"
#endif
#include "board_display.h"
#include "driver/uart.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lisp_runtime.h"
#include "repl_boot_info.h"
#include "repl_frontend.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

enum { REPL_STACK_BYTES = 6144, RX_BYTES = 2048, UART_EVENTS = 8 };
static const char *TAG = "lisp_repl";
static atomic_bool started;
static atomic_uint last_owner_tick;
static repl_frontend_t frontend;
static QueueHandle_t uart_events;
static void *runtime_memory;
#if CONFIG_WIFI_REPL_ENABLE
#include "repl_remote_logic.h"
#include "repl_tcp.h"
#include "wifi_service.h"
_Static_assert(sizeof(frontend.source) == REPL_REMOTE_SOURCE_CAPACITY,
               "Remote source must fit the owner buffer");
static QueueHandle_t remote_commands, remote_results;
static SemaphoreHandle_t remote_gate;
static atomic_bool remote_fault;
static bool remote_active; /* Owner task only. */
static struct {
    char source[REPL_REMOTE_SOURCE_CAPACITY];
    size_t length;
    repl_remote_reply_t reply;
    esp_err_t status;
} remote;

static void remote_cleanup(void)
{
    if (remote_commands != NULL)
        vQueueDelete(remote_commands);
    if (remote_results != NULL)
        vQueueDelete(remote_results);
    if (remote_gate != NULL)
        vSemaphoreDelete(remote_gate);
    remote_commands = NULL;
    remote_results = NULL;
    remote_gate = NULL;
}
static bool remote_initialize(void)
{
    remote_commands = xQueueCreate(1, sizeof(uint8_t));
    remote_results = xQueueCreate(1, sizeof(uint8_t));
    remote_gate = xSemaphoreCreateBinary();
    if (remote_commands == NULL || remote_results == NULL || remote_gate == NULL) {
        remote_cleanup();
        return false;
    }
    if (xSemaphoreGive(remote_gate) != pdTRUE) {
        remote_cleanup();
        return false;
    }
    return true;
}
esp_err_t lisp_repl_remote_eval(const char *source, size_t length, char *reply, size_t capacity)
{
    if (!repl_remote_source_valid(source, length) || reply == NULL || capacity < REPL_REMOTE_REPLY_CAPACITY)
        return ESP_ERR_INVALID_ARG;
    if (!lisp_repl_is_healthy() || remote_gate == NULL)
        return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(remote_gate, 0) != pdTRUE)
        return ESP_ERR_INVALID_STATE;
    memcpy(remote.source, source, length);
    remote.source[length] = 0;
    remote.length = length;
    uint8_t signal = 1;
    if (xQueueSend(remote_commands, &signal, 0) != pdTRUE ||
        xQueueReceive(remote_results, &signal, pdMS_TO_TICKS(8000)) != pdTRUE) {
        /* Never reuse shared storage while a late owner could still access it.
         * Binary gate remains taken until reboot; no stack pointer was shared. */
        atomic_store(&remote_fault, true);
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t result = remote.status;
    memcpy(reply, remote.reply.text, remote.reply.used + 1);
    (void)xSemaphoreGive(remote_gate);
    return result;
}
#else
esp_err_t lisp_repl_remote_eval(const char *source, size_t length, char *reply, size_t capacity)
{
    (void)source;
    (void)length;
    (void)reply;
    (void)capacity;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
static const char help[] = "LispBM RAM-only REPL; UART0 115200\n"
                           "Enter evaluates; unclosed forms continue.\n"
                           "BS/DEL edit; Ctrl-U clears this line.\n"
                           "Ctrl-C/:reset forget ALL RAM definitions.\n"
                           ":help help; :clear clears LCD output.\n"
                           "No save, Bluetooth, DSP or hardware IO.\n"
#if CONFIG_WIFI_REPL_ENABLE
                           "Wi-Fi: plain TCP2323; no TLS/token.\n"
#endif
    ;

/* Only REPL output goes to the LCD; ESP logs remain on the UART console.
 * Serial logs may interleave with REPL output, but never become Lisp input. */
static void serial_write(const char *text, size_t length)
{
    size_t first = 0;
    for (size_t i = 0; i < length; i++) {
        if (text[i] == '\n') {
            if (i > first)
                (void)uart_write_bytes(UART_NUM_0, text + first, i - first);
            (void)uart_write_bytes(UART_NUM_0, "\r\n", 2);
            first = i + 1;
        }
    }
    if (first < length)
        (void)uart_write_bytes(UART_NUM_0, text + first, length - first);
}
static void output(void *user, const char *text, size_t length)
{
    (void)user;
    lcd_terminal_write(&frontend.terminal, text, length);
#if CONFIG_WIFI_REPL_ENABLE
    if (remote_active)
        repl_remote_reply_append(&remote.reply, text, length);
#endif
    serial_write(text, length);
}
static void message(const char *text)
{
    output(NULL, text, strlen(text));
}
static void prompt(void)
{
    serial_write(frontend.line_start ? "...> " : "lbm> ", 5);
}
static void finish(void)
{
    repl_frontend_finish(&frontend);
    prompt();
}
static void input_byte(uint8_t byte)
{
#if CONFIG_WIFI_REPL_ENABLE
    if (remote_active)
        return; /* No UART bytes spliced into a network command. */
#endif
    size_t before = frontend.length;
    bool was_ready = frontend.ready;
    repl_input_event_t event = repl_frontend_feed(&frontend, byte);
    if (event == REPL_CANCELLED) {
        serial_write("^C\n", 3);
        if (lisp_runtime_reset())
            message("RAM definitions cleared\n");
        else
            message("ERROR: runtime reset failed\n");
        prompt();
    } else if (event == REPL_OVERFLOW) {
        /* Never evaluate a silently truncated expression after RX/input overflow. */
        serial_write("\a", 1);
        lcd_terminal_input(&frontend.terminal, "error> ", "input full; Ctrl-U/Ctrl-C", 24);
    } else if (event == REPL_READY) {
        serial_write("\n", 1);
        if (!strcmp(frontend.source, ":help")) {
            message(help);
            finish();
        } else if (!strcmp(frontend.source, ":clear")) {
            lcd_terminal_init(&frontend.terminal);
            finish();
        } else if (!strcmp(frontend.source, ":reset")) {
            message(lisp_runtime_reset() ? "RAM definitions cleared\n" : "ERROR: runtime reset failed\n");
            finish();
        } else if (frontend.source[0] == ':') {
            message("ERROR: unknown command; :help\n");
            finish();
        } else if (!lisp_runtime_submit(frontend.source, frontend.length,
                                        (uint64_t)(esp_timer_get_time() / 1000)))
            finish();
    } else if (event == REPL_CHANGED) {
        if (byte == '\r' || byte == '\n') {
            serial_write("\n", 1);
            prompt();
        } else if (byte == 8 || byte == 127)
            serial_write("\b \b", 3);
        else if (byte == 21) {
            for (size_t i = frontend.length; i < before; i++)
                serial_write("\b \b", 3);
        } else if (frontend.length > before) {
            char echo = frontend.source[frontend.length - 1];
            serial_write(&echo, 1);
        }
    } else if (was_ready && byte >= 32)
        serial_write("\a", 1); /* One submission at a time. */
}
#if CONFIG_WIFI_REPL_ENABLE
static void remote_complete(void)
{
    remote_active = false;
    repl_remote_reply_finish(&remote.reply);
    uint8_t signal = 1;
    if (xQueueSend(remote_results, &signal, 0) != pdTRUE)
        atomic_store(&remote_fault, true);
}
static void remote_poll(void)
{
    uint8_t signal;
    if (xQueueReceive(remote_commands, &signal, 0) != pdTRUE)
        return;
    repl_remote_reply_init(&remote.reply);
    if (!lisp_runtime_ready() || lisp_runtime_busy() || frontend.ready || frontend.length != 0) {
        remote.status = ESP_ERR_INVALID_STATE;
        remote_complete();
        return;
    }
    memcpy(frontend.source, remote.source, remote.length + 1);
    frontend.length = remote.length;
    frontend.line_start = 0;
    frontend.ready = true;
    lcd_terminal_input(&frontend.terminal, "eval> ", "", 0);
    message("wifi> ");
    message(remote.source);
    message("\n");
    remote_active = true;
    remote.status = ESP_OK;
    if (!strcmp(remote.source, ":help"))
        message(help);
    else if (!strcmp(remote.source, ":clear")) {
        lcd_terminal_init(&frontend.terminal);
        message("LCD cleared\n");
    } else if (!strcmp(remote.source, ":reset")) {
        if (lisp_runtime_reset())
            message("RAM definitions cleared\n");
        else {
            remote.status = ESP_FAIL;
            message("ERROR: runtime reset failed\n");
        }
    } else if (remote.source[0] == ':')
        message("ERROR: unknown command; :help\n");
    else
        (void)lisp_runtime_submit(remote.source, remote.length, (uint64_t)(esp_timer_get_time() / 1000));
    /* All evaluations, including TCP input, are stepped by this same owner. */
}
#endif
#if CONFIG_WIFI_REPL_ENABLE
static void report_connection(bool *reported)
{
    if (*reported || remote_active || lisp_runtime_busy() || frontend.length != 0 || frontend.ready)
        return;
    wifi_link_snapshot_t link;
    if (!repl_tcp_is_healthy() || !wifi_service_get_link_snapshot(&link) || link.phase != WIFI_LINK_ONLINE)
        return;
    char text[REPL_BOOT_INFO_CAPACITY];
    bool ota = false;
#if CONFIG_OTA_SERVICE_ENABLE
    ota = true;
#endif
    if (repl_network_banner(text, sizeof(text), link.ipv4, ota)) {
        message(text);
        prompt();
        *reported = true; /* Once at startup, never periodic terminal output. */
    }
}
#endif
static void repl_task(void *argument)
{
    (void)argument;
    repl_frontend_init(&frontend);
    if (!lisp_runtime_init(runtime_memory, lisp_runtime_storage_size(), output, NULL)) {
        char detail[96];
        (void)snprintf(detail, sizeof(detail), "ERROR: LispBM init failed at %s; align_mod8=%u\n",
                       lisp_runtime_init_stage(), (unsigned)((uintptr_t)runtime_memory % 8));
        message(detail);
        (void)board_display_terminal_update(&frontend.terminal);
        vTaskDelete(NULL);
        return; /* Allocations quarantined; no automatic restart. */
    }
    bool probe_started = lisp_runtime_selftest_start((uint64_t)(esp_timer_get_time() / 1000));
    while (probe_started && lisp_runtime_busy()) {
        lisp_runtime_step((uint64_t)(esp_timer_get_time() / 1000));
        vTaskDelay(1);
    }
    if (!probe_started || !lisp_runtime_selftest_passed()) {
        message("ERROR: LispBM startup probe failed; REPL stopped\n");
        (void)board_display_terminal_update(&frontend.terminal);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "init_stage=%s align_mod8=%u selftest=PASS; ready for input", lisp_runtime_init_stage(),
             (unsigned)((uintptr_t)runtime_memory % 8));
    char banner[REPL_BOOT_INFO_CAPACITY];
    bool wifi = false;
#if CONFIG_WIFI_REPL_ENABLE
    wifi = true;
#endif
    bool usb_updates = false;
#if CONFIG_SINGLE_APP_USB_ENABLE
    usb_updates = true;
#endif
    if (repl_boot_banner(banner, sizeof(banner), esp_app_get_description()->version, wifi, usb_updates))
        message(banner);
    prompt();
#if CONFIG_WIFI_REPL_ENABLE
    bool connection_reported = false;
#endif
    for (;;) {
#if CONFIG_WIFI_REPL_ENABLE
        report_connection(&connection_reported);
        remote_poll();
#endif
        uart_event_t event;
        bool damaged = false;
        for (unsigned i = 0; i < UART_EVENTS && xQueueReceive(uart_events, &event, 0) == pdTRUE; i++) {
            if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL ||
                event.type == UART_FRAME_ERR || event.type == UART_PARITY_ERR)
                damaged = true;
        }
        if (damaged) {
            (void)uart_flush_input(UART_NUM_0);
#if CONFIG_WIFI_REPL_ENABLE
            if (!remote_active)
#endif
            {
                repl_frontend_finish(&frontend);
                if (!lisp_runtime_reset())
                    message("ERROR: runtime reset failed\n");
                message("ERROR: UART input lost; RAM definitions cleared\n");
                prompt();
            }
        } else {
            uint8_t bytes[64];
            int count = uart_read_bytes(UART_NUM_0, bytes, sizeof(bytes), 0);
            for (int i = 0; i < count; i++)
                input_byte(bytes[i]);
        }
        bool was_busy = lisp_runtime_busy();
        lisp_runtime_step((uint64_t)(esp_timer_get_time() / 1000));
#if CONFIG_WIFI_REPL_ENABLE
        if (remote_active && !lisp_runtime_busy()) {
            remote_complete();
            finish();
        } else
#endif
            if (was_busy && !lisp_runtime_busy())
            finish();
        (void)board_display_terminal_update(&frontend.terminal);
        atomic_store(&last_owner_tick, lisp_runtime_ready() && lisp_runtime_selftest_passed()
                                           ? (unsigned)xTaskGetTickCount()
                                           : 0);
        vTaskDelay(1); /* Always yield: heartbeat/LCD/OTA must stay independent. */
    }
}
bool lisp_repl_is_healthy(void)
{
    unsigned last = atomic_load(&last_owner_tick);
    bool recent = last != 0 && (TickType_t)(xTaskGetTickCount() - last) <= pdMS_TO_TICKS(3000);
#if CONFIG_WIFI_REPL_ENABLE
    return recent && !atomic_load(&remote_fault) && repl_tcp_is_healthy();
#else
    return recent;
#endif
}
esp_err_t lisp_repl_start(void)
{
    bool expected = false;
    if (!atomic_compare_exchange_strong(&started, &expected, true))
        return ESP_ERR_INVALID_STATE;
    /* No PSRAM fallback that could starve internal DMA/Wi-Fi/FreeRTOS memory. */
    runtime_memory =
        heap_caps_aligned_calloc(lisp_runtime_storage_alignment(), 1, lisp_runtime_storage_size(),
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (runtime_memory == NULL)
        return ESP_ERR_NO_MEM;
    esp_err_t error = uart_driver_install(UART_NUM_0, RX_BYTES, 0, UART_EVENTS, &uart_events, 0);
    if (error != ESP_OK) {
        heap_caps_free(runtime_memory);
        runtime_memory = NULL;
        return error;
    }
#if CONFIG_WIFI_REPL_ENABLE
    if (!remote_initialize()) {
        (void)uart_driver_delete(UART_NUM_0);
        heap_caps_free(runtime_memory);
        runtime_memory = NULL;
        return ESP_ERR_NO_MEM;
    }
#endif
    /* Console baud and pins are unchanged; no new GPIO or NVS configuration. */
    if (xTaskCreate(repl_task, "lisp_repl", REPL_STACK_BYTES, NULL, 1, NULL) != pdPASS) {
#if CONFIG_WIFI_REPL_ENABLE
        remote_cleanup();
#endif
        (void)uart_driver_delete(UART_NUM_0);
        heap_caps_free(runtime_memory);
        runtime_memory = NULL;
        return ESP_ERR_NO_MEM;
    }
#if CONFIG_WIFI_REPL_ENABLE
    /* On failure owner allocations stay alive; health remains false for rollback. */
    esp_err_t tcp_error = repl_tcp_start();
    if (tcp_error != ESP_OK)
        return tcp_error;
#endif
    ESP_LOGI(TAG, "LispBM owner started; PSRAM=%u bytes; hardware REPL NOT TESTED",
             (unsigned)lisp_runtime_storage_size());
    return ESP_OK;
}
#else
esp_err_t lisp_repl_remote_eval(const char *source, size_t length, char *reply, size_t capacity)
{
    (void)source;
    (void)length;
    (void)reply;
    (void)capacity;
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t lisp_repl_start(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}
bool lisp_repl_is_healthy(void)
{
    return false;
}
#endif
