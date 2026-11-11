#include "lisp_perf_stats.h"
#include <stddef.h>
#include <string.h>

bool lisp_perf_stats_start(lisp_perf_stats_t *state)
{
    if (!state || state->enabled || state->generation == UINT32_MAX)
        return false;
    uint32_t next = state->generation + 1;
    memset(state, 0, sizeof(*state));
    state->generation = next;
    state->enabled = true;
    return true;
}
bool lisp_perf_stats_stop(lisp_perf_stats_t *state, lisp_perf_stats_t *snapshot)
{
    if (!state || !snapshot || snapshot == state || !state->enabled)
        return false;
    state->enabled = false;
    *snapshot = *state;
    return true;
}
lisp_perf_token_t lisp_perf_stats_begin(const lisp_perf_stats_t *state, uint64_t now_us)
{
    return (lisp_perf_token_t){now_us, state ? state->generation : 0, state && state->enabled};
}
bool lisp_perf_stats_record(lisp_perf_stats_t *state, lisp_perf_metric_t metric, uint64_t duration_us)
{
    if (!state || !state->enabled || (unsigned)metric >= LISP_PERF_METRICS)
        return false;
    lisp_perf_metric_stats_t *entry = &state->metrics[metric];
    if (entry->count == UINT32_MAX)
        return false;
    if (entry->count == 0 || duration_us < entry->min_us)
        entry->min_us = duration_us;
    if (duration_us > entry->max_us)
        entry->max_us = duration_us;
    entry->total_us = UINT64_MAX - entry->total_us < duration_us ? UINT64_MAX : entry->total_us + duration_us;
    entry->count++;
    return true;
}
bool lisp_perf_stats_end(lisp_perf_stats_t *state, lisp_perf_metric_t metric, lisp_perf_token_t token,
                         uint64_t now_us)
{
    if (!state || !state->enabled || !token.active || state->generation != token.generation ||
        now_us < token.started_us)
        return false;
    return lisp_perf_stats_record(state, metric, now_us - token.started_us);
}
