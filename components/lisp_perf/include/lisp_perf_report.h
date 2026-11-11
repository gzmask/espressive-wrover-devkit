#pragma once
#include "lisp_perf_stats.h"
#include <stddef.h>

typedef enum { LISP_PERF_COMMAND_NONE, LISP_PERF_COMMAND_START, LISP_PERF_COMMAND_STOP } lisp_perf_command_t;
lisp_perf_command_t lisp_perf_command(const char *source);
/* Returns false on invalid arguments or truncation; no allocation. */
bool lisp_perf_format_line(const lisp_perf_stats_t *snapshot, lisp_perf_metric_t metric, char *out,
                           size_t capacity);
