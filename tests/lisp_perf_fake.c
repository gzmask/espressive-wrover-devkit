#include "lisp_perf.h"

static lisp_perf_stats_t state;
static uint64_t now_us;

uint64_t lisp_perf_now_us(void)
{
    return ++now_us;
}
bool lisp_perf_start(void)
{
    return lisp_perf_stats_start(&state);
}
bool lisp_perf_stop(lisp_perf_stats_t *snapshot)
{
    return lisp_perf_stats_stop(&state, snapshot);
}
lisp_perf_token_t lisp_perf_begin(void)
{
    return lisp_perf_stats_begin(&state, lisp_perf_now_us());
}
void lisp_perf_end(lisp_perf_metric_t metric, lisp_perf_token_t token)
{
    (void)lisp_perf_stats_end(&state, metric, token, lisp_perf_now_us());
}
void lisp_perf_add(lisp_perf_metric_t metric, lisp_perf_token_t token, uint64_t duration_us)
{
    if (token.active && token.generation == state.generation)
        (void)lisp_perf_stats_record(&state, metric, duration_us);
}
void lisp_perf_record(lisp_perf_metric_t metric, uint64_t duration_us)
{
    (void)lisp_perf_stats_record(&state, metric, duration_us);
}
