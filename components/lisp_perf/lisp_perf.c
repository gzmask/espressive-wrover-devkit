#include "lisp_perf.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static lisp_perf_stats_t state;

uint64_t lisp_perf_now_us(void)
{
    return (uint64_t)esp_timer_get_time();
}
bool lisp_perf_start(void)
{
    portENTER_CRITICAL(&lock);
    bool result = lisp_perf_stats_start(&state);
    portEXIT_CRITICAL(&lock);
    return result;
}
bool lisp_perf_stop(lisp_perf_stats_t *snapshot)
{
    portENTER_CRITICAL(&lock);
    bool result = lisp_perf_stats_stop(&state, snapshot);
    portEXIT_CRITICAL(&lock);
    return result;
}
lisp_perf_token_t lisp_perf_begin(void)
{
    uint64_t now_us = lisp_perf_now_us();
    portENTER_CRITICAL(&lock);
    lisp_perf_token_t token = lisp_perf_stats_begin(&state, now_us);
    portEXIT_CRITICAL(&lock);
    return token;
}
void lisp_perf_end(lisp_perf_metric_t metric, lisp_perf_token_t token)
{
    if (!token.active)
        return;
    uint64_t now_us = lisp_perf_now_us();
    portENTER_CRITICAL(&lock);
    (void)lisp_perf_stats_end(&state, metric, token, now_us);
    portEXIT_CRITICAL(&lock);
}
void lisp_perf_add(lisp_perf_metric_t metric, lisp_perf_token_t token, uint64_t duration_us)
{
    if (!token.active)
        return;
    portENTER_CRITICAL(&lock);
    if (state.enabled && token.generation == state.generation)
        (void)lisp_perf_stats_record(&state, metric, duration_us);
    portEXIT_CRITICAL(&lock);
}
void lisp_perf_record(lisp_perf_metric_t metric, uint64_t duration_us)
{
    portENTER_CRITICAL(&lock);
    (void)lisp_perf_stats_record(&state, metric, duration_us);
    portEXIT_CRITICAL(&lock);
}
