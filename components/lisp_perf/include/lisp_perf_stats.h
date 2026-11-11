#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LISP_PERF_CLEAR,
    LISP_PERF_RECT,
    LISP_PERF_TEXT,
    LISP_PERF_CANVAS_COPY,
    LISP_PERF_CALLBACK,
    LISP_PERF_TICK_LATE,
    LISP_PERF_BACKPRESSURE,
    LISP_PERF_LCD_FRAME,
    LISP_PERF_LCD_RENDER,
    LISP_PERF_LCD_TRANSFER,
    LISP_PERF_METRICS
} lisp_perf_metric_t;
typedef struct {
    uint64_t total_us, min_us, max_us;
    uint32_t count;
} lisp_perf_metric_stats_t;
typedef struct {
    lisp_perf_metric_stats_t metrics[LISP_PERF_METRICS];
    uint32_t generation;
    bool enabled;
} lisp_perf_stats_t;
typedef struct {
    uint64_t started_us;
    uint32_t generation;
    bool active;
} lisp_perf_token_t;

/* Pure state, no internal locking. The platform wrapper serializes all calls. */
bool lisp_perf_stats_start(lisp_perf_stats_t *state);
bool lisp_perf_stats_stop(lisp_perf_stats_t *state, lisp_perf_stats_t *snapshot);
lisp_perf_token_t lisp_perf_stats_begin(const lisp_perf_stats_t *state, uint64_t now_us);
bool lisp_perf_stats_end(lisp_perf_stats_t *state, lisp_perf_metric_t metric, lisp_perf_token_t token,
                         uint64_t now_us);
bool lisp_perf_stats_record(lisp_perf_stats_t *state, lisp_perf_metric_t metric, uint64_t duration_us);
