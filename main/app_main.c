#include <inttypes.h>
#include <stdatomic.h>

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "node_status.h"
#include "sdkconfig.h"

#if CONFIG_BOARD_DISPLAY_ENABLE
#include "board_display.h"
#endif

#if CONFIG_LISP_REPL_ENABLE
#include "lisp_repl.h"
#endif

#if CONFIG_WIFI_SERVICE_ENABLE
#include "wifi_service.h"
#endif

#if CONFIG_SPIRAM
#include "esp_psram.h"
#include "psram_probe.h"
#endif

#if CONFIG_OTA_SERVICE_ENABLE
#include "ota_service.h"
static atomic_uint last_heartbeat_tick;
static bool startup_healthy;
static bool ota_health(void)
{
    unsigned last = atomic_load(&last_heartbeat_tick);
    bool healthy = startup_healthy && last != 0 &&
                   (TickType_t)(xTaskGetTickCount() - last) <= pdMS_TO_TICKS(3000) &&
                   board_display_is_healthy();
#if CONFIG_LISP_REPL_ENABLE
    return healthy && lisp_repl_is_healthy();
#else
    return healthy;
#endif
}
#endif

static const char *TAG = "radio_node";

/* ESP-IDF xTaskCreate stack sizes and high-water marks are BYTES, not words. */
enum {
    TASK_STACK_BYTES = 4096,
    SAMPLE_QUEUE_LENGTH = 4,
    HEARTBEAT_PERIOD_MS = 1000,
    HEARTBEAT_TIMEOUT_MS = 3000,
    STATUS_LOG_PERIOD_MS = 5000,
};

static void heartbeat_task(void *argument)
{
    QueueHandle_t samples = (QueueHandle_t)argument;
    for (;;) {
        node_sample_t sample = {
            .uptime_ms = (uint64_t)(esp_timer_get_time() / 1000),
            .free_heap_bytes = esp_get_free_heap_size(),
            .min_free_heap_bytes = esp_get_minimum_free_heap_size(),
            .free_internal_heap_bytes =
                (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        };
        if (xQueueSend(samples, &sample, 0) != pdPASS) {
            /* Bounded queue: never block sampling indefinitely behind logging. */
            ESP_LOGW(TAG, "sample queue full; heartbeat sample dropped");
        }
#if CONFIG_OTA_SERVICE_ENABLE
        else {
            atomic_store(&last_heartbeat_tick, (unsigned)xTaskGetTickCount());
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS));
    }
}

static void log_status(const node_status_t *status)
{
    char json[512];
    if (node_status_format_json(status, esp_app_get_description()->version, json, sizeof(json))) {
        ESP_LOGI(TAG, "status=%s", json);
    } else {
        ESP_LOGE(TAG, "status serialization failed (allocation/buffer/state)");
    }
    ESP_LOGI(TAG, "status task stack low-water=%u bytes", (unsigned)uxTaskGetStackHighWaterMark(NULL));
}

static void status_task(void *argument)
{
    QueueHandle_t samples = (QueueHandle_t)argument;
    node_status_t status = node_status_initial();
    uint64_t last_log_ms = 0;
    log_status(&status);
    for (;;) {
        node_sample_t sample;
        if (xQueueReceive(samples, &sample, pdMS_TO_TICKS(HEARTBEAT_TIMEOUT_MS)) != pdPASS) {
            node_status_mark_stalled(&status);
            log_status(&status);
            continue;
        }
        if (!node_status_receive(&status, &sample)) {
            ESP_LOGW(TAG, "ignored duplicate/out-of-order heartbeat sample");
            continue;
        }
        if (status.heartbeat_count == 1 || sample.uptime_ms - last_log_ms >= STATUS_LOG_PERIOD_MS) {
            /* Receiving an increasing count proves task-to-task queue traffic. */
            log_status(&status);
            last_log_ms = sample.uptime_ms;
        }
    }
}

static void log_boot_diagnostics(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "WROVER RADIO NODE firmware=%s idf=%s target=%s", app->version, esp_get_idf_version(),
             CONFIG_IDF_TARGET);
    ESP_LOGI(TAG, "boot elapsed=%" PRId64 " ms reset_reason=%d chip_rev=%u.%u cores=%u",
             esp_timer_get_time() / 1000, (int)esp_reset_reason(), (unsigned)(chip.revision / 100),
             (unsigned)(chip.revision % 100), (unsigned)chip.cores);
    uint32_t flash_bytes = 0;
    esp_err_t error = esp_flash_get_size(NULL, &flash_bytes);
    if (error == ESP_OK) {
        ESP_LOGI(TAG, "detected flash=%" PRIu32 " bytes", flash_bytes);
    } else {
        ESP_LOGE(TAG, "flash size query failed: %s", esp_err_to_name(error));
    }
    ESP_LOGI(TAG, "free heap=%" PRIu32 " bytes internal=%u bytes minimum=%" PRIu32 " bytes",
             esp_get_free_heap_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             esp_get_minimum_free_heap_size());
#if CONFIG_SPIRAM
    if (esp_psram_is_initialized()) {
        ESP_LOGI(TAG, "PSRAM initialized: device=%u bytes heap=%u bytes free=%u bytes",
                 (unsigned)esp_psram_get_size(),
                 (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    } else {
        ESP_LOGW(TAG, "PSRAM support enabled but initialization unavailable/failed; size unknown");
    }
#else
    ESP_LOGI(TAG, "PSRAM not configured: presence and size NOT TESTED");
#endif
#if CONFIG_BOARD_DISPLAY_ENABLE
    ESP_LOGI(TAG, "LCD diagnostic configured");
#else
    ESP_LOGI(TAG, "LCD not initialized");
#endif
#if CONFIG_WIFI_SERVICE_ENABLE
#if CONFIG_WIFI_SERVICE_CONNECT_ENABLE
    ESP_LOGI(TAG, "Wi-Fi station configured; US; embedded credentials; NVS persistence disabled");
#else
    ESP_LOGI(TAG, "Wi-Fi passive scan configured; NVS persistence/association/HTTP disabled");
#endif
#else
    ESP_LOGI(TAG, "Wi-Fi/HTTP disabled");
#endif
}

void app_main(void)
{
    log_boot_diagnostics();
    QueueHandle_t samples = xQueueCreate(SAMPLE_QUEUE_LENGTH, sizeof(node_sample_t));
    if (samples == NULL) {
        ESP_LOGE(TAG, "cannot allocate heartbeat queue; tasks not started");
#if CONFIG_OTA_SERVICE_ENABLE
        esp_restart();
#endif
        return;
    }
    if (xTaskCreate(status_task, "node_status", TASK_STACK_BYTES, samples, 2, NULL) != pdPASS) {
        ESP_LOGE(TAG, "cannot create status task; tasks not started");
        vQueueDelete(samples);
#if CONFIG_OTA_SERVICE_ENABLE
        esp_restart();
#endif
        return;
    }
    if (xTaskCreate(heartbeat_task, "node_heartbeat", TASK_STACK_BYTES, samples, 2, NULL) != pdPASS) {
        /* Keep the consumer alive so it reports the missing heartbeat as
         * stalled. Do not delete a task that might be logging on another core. */
        ESP_LOGE(TAG, "cannot create heartbeat task; status task will report stalled");
#if CONFIG_OTA_SERVICE_ENABLE
        esp_restart();
#endif
        return;
    }
    ESP_LOGI(TAG, "two FreeRTOS tasks started; heartbeat=1 Hz; status log=5 s; queue=%d",
             SAMPLE_QUEUE_LENGTH);
#if CONFIG_SPIRAM
#if CONFIG_OTA_SERVICE_ENABLE
    startup_healthy = psram_probe_run();
#else
    (void)psram_probe_run();
#endif
#endif
#if CONFIG_BOARD_DISPLAY_ENABLE
    esp_err_t display_error = board_display_start();
    if (display_error != ESP_OK) {
        ESP_LOGE(TAG, "LCD task start failed: %s; heartbeat remains active", esp_err_to_name(display_error));
#if CONFIG_OTA_SERVICE_ENABLE
        startup_healthy = false;
#endif
    }
#endif
#if CONFIG_LISP_REPL_ENABLE
    esp_err_t repl_error = lisp_repl_start();
    if (repl_error != ESP_OK) {
        ESP_LOGE(TAG, "REPL startup failed: %s; heartbeat remains active", esp_err_to_name(repl_error));
#if CONFIG_OTA_SERVICE_ENABLE
        startup_healthy = false;
#endif
    }
#endif
#if CONFIG_WIFI_SERVICE_ENABLE
    esp_err_t wifi_error = wifi_service_start();
    if (wifi_error != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi task start failed: %s; heartbeat remains active", esp_err_to_name(wifi_error));
    }
#endif
#if CONFIG_OTA_SERVICE_ENABLE
    esp_err_t ota_error = ota_service_start(ota_health);
    if (ota_error != ESP_OK) {
        ESP_LOGE(TAG, "OTA monitor startup failed: %s", esp_err_to_name(ota_error));
        esp_restart();
    }
#endif
}
