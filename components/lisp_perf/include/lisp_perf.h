#pragma once
#include "lisp_perf_stats.h"

/* Thread-safe task-context API. Only compiled into the opt-in diagnostic build.
 * begin/end spans are discarded across start/stop generations. Never call from ISR. */
bool lisp_perf_start(void);
bool lisp_perf_stop(lisp_perf_stats_t *snapshot);
lisp_perf_token_t lisp_perf_begin(void);
void lisp_perf_end(lisp_perf_metric_t metric, lisp_perf_token_t token);
void lisp_perf_add(lisp_perf_metric_t metric, lisp_perf_token_t token, uint64_t duration_us);
void lisp_perf_record(lisp_perf_metric_t metric, uint64_t duration_us);
uint64_t lisp_perf_now_us(void);
