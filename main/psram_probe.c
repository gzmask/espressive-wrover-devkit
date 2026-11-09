#include "psram_probe.h"

#include "sdkconfig.h"

#if CONFIG_SPIRAM
#include <inttypes.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "memory_pattern.h"

#if CONFIG_ESP32_REV_MIN_FULL < 300 && !CONFIG_SPIRAM_CACHE_WORKAROUND
#error "Classic ESP32 rev1 PSRAM probing requires CONFIG_SPIRAM_CACHE_WORKAROUND"
#endif

static const char *TAG = "psram_probe";

static bool check_buffer(volatile uint32_t *words, size_t count)
{
    const uint32_t seeds[] = {0,
                              UINT32_MAX,
                              UINT32_C(0xaaaaaaaa),
                              UINT32_C(0x55555555),
                              UINT32_C(0x01234567),
                              UINT32_C(0xfedcba98)};
    for (size_t pass = 0; pass < sizeof(seeds) / sizeof(seeds[0]); pass++) {
        if (!memory_pattern_fill(words, count, seeds[pass])) {
            ESP_LOGE(TAG, "self-test FAIL: invalid test range");
            return false;
        }
        /* The full 128 KiB write exceeds the 32 KiB cache; this is not a test
         * that only reads a small, just-written cache line. Both tasks can run. */
        vTaskDelay(1);
        size_t bad_word = SIZE_MAX;
        memory_pattern_result_t result = memory_pattern_check(words, count, seeds[pass], &bad_word);
        if (result != MEMORY_PATTERN_OK) {
            ESP_LOGE(TAG, "self-test FAIL: pass=%u word=%u result=%d", (unsigned)pass + 1, (unsigned)bad_word,
                     (int)result);
            return false;
        }
        ESP_LOGI(TAG, "self-test pass=%u/6 seed=%08" PRIx32 " OK", (unsigned)pass + 1, seeds[pass]);
        vTaskDelay(1);
    }
    return true;
}
#endif

bool psram_probe_run(void)
{
#if CONFIG_SPIRAM
    if (!esp_psram_is_initialized()) {
        ESP_LOGW(TAG, "self-test NOT TESTED: PSRAM initialization unavailable; presence/size unknown");
        return false;
    }
    const uint32_t caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    const size_t test_bytes = 128 * 1024;
    ESP_LOGI(TAG, "device_size=%u bytes allocator_total=%u bytes allocator_free=%u bytes",
             (unsigned)esp_psram_get_size(), (unsigned)heap_caps_get_total_size(caps),
             (unsigned)heap_caps_get_free_size(caps));
    ESP_LOGI(TAG, "clock=%d MHz CLK=GPIO%d CS=GPIO%d; testing only allocated RAM", CONFIG_SPIRAM_SPEED,
             CONFIG_D0WD_PSRAM_CLK_IO, CONFIG_D0WD_PSRAM_CS_IO);
    uint32_t *words = heap_caps_malloc(test_bytes, caps);
    if (words == NULL) {
        ESP_LOGE(TAG, "self-test NOT TESTED: cannot allocate %u external bytes", (unsigned)test_bytes);
        return false;
    }
    if (!esp_ptr_external_ram(words)) {
        ESP_LOGE(TAG, "self-test FAIL: allocation is not in external RAM");
        heap_caps_free(words);
        return false;
    }
    bool passed = check_buffer(words, test_bytes / sizeof(*words));
    heap_caps_free(words);
    if (passed) {
        ESP_LOGI(TAG, "self-test PASS: %u bytes, 6 patterns; buffer released; external_free=%u bytes",
                 (unsigned)test_bytes, (unsigned)heap_caps_get_free_size(caps));
    } else {
        ESP_LOGE(TAG, "self-test FAIL: buffer released; external_free=%u bytes",
                 (unsigned)heap_caps_get_free_size(caps));
    }
    return passed;
#else
    return true;
#endif
}
