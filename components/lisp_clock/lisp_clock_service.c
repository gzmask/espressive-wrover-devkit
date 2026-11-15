#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lisp_clock.h"
#include <stdatomic.h>
#include <time.h>
static atomic_flag sync_lock = ATOMIC_FLAG_INIT;
typedef struct {
    bool (*cancelled)(void *);
    void *user;
} request_t;
static uint64_t now_ms(void *u)
{
    (void)u;
    return (uint64_t)(esp_timer_get_time() / 1000);
}
static int64_t unix_seconds(void *u)
{
    (void)u;
    return (int64_t)time(NULL);
}
static bool cancelled(void *u)
{
    request_t *r = u;
    return r->cancelled(r->user);
}
static bool try_lock(void *u)
{
    (void)u;
    return !atomic_flag_test_and_set(&sync_lock);
}
static void unlock(void *u)
{
    (void)u;
    atomic_flag_clear(&sync_lock);
}
static bool start(void *u)
{
    (void)u;
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    return esp_netif_sntp_init(&config) == ESP_OK;
}
static bool wait_sync(void *u, unsigned ms)
{
    (void)u;
    TickType_t ticks = pdMS_TO_TICKS(ms);
    return esp_netif_sntp_sync_wait(ticks ? ticks : 1) == ESP_OK;
}
static void stop(void *u)
{
    (void)u;
    esp_netif_sntp_deinit();
}
static void sleep_ms(void *u, unsigned ms)
{
    (void)u;
    TickType_t ticks = pdMS_TO_TICKS(ms);
    vTaskDelay(ticks ? ticks : 1);
}
bool lisp_clock_service_ensure(uint64_t deadline_ms, bool (*is_cancelled)(void *), void *user)
{
    if (!is_cancelled)
        return false;
    request_t request = {is_cancelled, user};
    const lisp_clock_io_t io = {&request, now_ms, unix_seconds, cancelled, try_lock,
                                unlock,   start,  wait_sync,    stop,      sleep_ms};
    return lisp_clock_ensure(&io, deadline_ms);
}
