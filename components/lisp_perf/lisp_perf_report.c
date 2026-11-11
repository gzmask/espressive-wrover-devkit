#include "lisp_perf_report.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

lisp_perf_command_t lisp_perf_command(const char *source)
{
    if (source && !strcmp(source, ":perf-start"))
        return LISP_PERF_COMMAND_START;
    if (source && !strcmp(source, ":perf-stop"))
        return LISP_PERF_COMMAND_STOP;
    return LISP_PERF_COMMAND_NONE;
}
bool lisp_perf_format_line(const lisp_perf_stats_t *snapshot, lisp_perf_metric_t metric, char *out,
                           size_t capacity)
{
    static const char *const names[LISP_PERF_METRICS] = {
        "clear",     "rect",         "text",      "canvas_copy", "callback",
        "tick_late", "backpressure", "lcd_frame", "lcd_render",  "lcd_transfer"};
    if (!snapshot || (unsigned)metric >= LISP_PERF_METRICS || !out || capacity == 0)
        return false;
    const lisp_perf_metric_stats_t *item = &snapshot->metrics[metric];
    int written = snprintf(
        out, capacity, "%s n=%" PRIu32 " avg=%" PRIu64 " min=%" PRIu64 " max=%" PRIu64 "\n", names[metric],
        item->count, item->count ? item->total_us / item->count : 0, item->min_us, item->max_us);
    return written >= 0 && (size_t)written < capacity;
}
