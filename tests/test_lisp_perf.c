#include "lisp_perf_report.h"
#include "lisp_perf_stats.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    assert(lisp_perf_command(NULL) == LISP_PERF_COMMAND_NONE);
    assert(lisp_perf_command(":perf-start") == LISP_PERF_COMMAND_START);
    assert(lisp_perf_command(":perf-stop") == LISP_PERF_COMMAND_STOP);
    assert(lisp_perf_command(":perf-start ") == LISP_PERF_COMMAND_NONE);
    lisp_perf_stats_t state = {0}, snapshot = {0};
    lisp_perf_token_t off = lisp_perf_stats_begin(&state, 10);
    assert(!off.active);
    assert(!lisp_perf_stats_stop(&state, &snapshot));
    assert(lisp_perf_stats_start(&state));
    assert(!lisp_perf_stats_start(&state));
    lisp_perf_token_t first = lisp_perf_stats_begin(&state, 100);
    assert(first.active);
    assert(lisp_perf_stats_end(&state, LISP_PERF_CLEAR, first, 150));
    lisp_perf_token_t second = lisp_perf_stats_begin(&state, 200);
    assert(lisp_perf_stats_end(&state, LISP_PERF_CLEAR, second, 280));
    assert(!lisp_perf_stats_end(&state, LISP_PERF_CLEAR, second, 199));
    assert(!lisp_perf_stats_end(&state, LISP_PERF_METRICS, second, 300));
    assert(lisp_perf_stats_record(&state, LISP_PERF_LCD_FRAME, 900));
    lisp_perf_token_t in_flight = lisp_perf_stats_begin(&state, 300);
    assert(lisp_perf_stats_stop(&state, &snapshot));
    assert(!state.enabled && snapshot.metrics[LISP_PERF_CLEAR].count == 2);
    assert(snapshot.metrics[LISP_PERF_CLEAR].total_us == 130);
    assert(snapshot.metrics[LISP_PERF_CLEAR].min_us == 50 && snapshot.metrics[LISP_PERF_CLEAR].max_us == 80);
    assert(snapshot.metrics[LISP_PERF_LCD_FRAME].total_us == 900);
    char line[160];
    assert(lisp_perf_format_line(&snapshot, LISP_PERF_CLEAR, line, sizeof(line)));
    assert(!strcmp(line, "clear n=2 avg=65 min=50 max=80\n"));
    assert(lisp_perf_format_line(&snapshot, LISP_PERF_RECT, line, sizeof(line)));
    assert(!strcmp(line, "rect n=0 avg=0 min=0 max=0\n"));
    assert(!lisp_perf_format_line(&snapshot, LISP_PERF_CLEAR, line, 4));
    assert(!lisp_perf_format_line(&snapshot, LISP_PERF_METRICS, line, sizeof(line)));
    assert(!lisp_perf_format_line(NULL, LISP_PERF_CLEAR, line, sizeof(line)));
    lisp_perf_stats_t worst = {0};
    size_t reply_bytes = strlen("Perf stopped; wall microseconds, not CPU\n");
    for (unsigned i = 0; i < LISP_PERF_METRICS; i++) {
        worst.metrics[i] = (lisp_perf_metric_stats_t){UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT32_MAX};
        assert(lisp_perf_format_line(&worst, (lisp_perf_metric_t)i, line, sizeof(line)));
        reply_bytes += strlen(line);
    }
    assert(reply_bytes < 2048); /* Entire report must fit the remote reply. */
    assert(!lisp_perf_stats_end(&state, LISP_PERF_CLEAR, in_flight, 400));
    assert(lisp_perf_stats_start(&state));
    assert(!lisp_perf_stats_end(&state, LISP_PERF_CLEAR, in_flight, 450));
    assert(!lisp_perf_stats_end(&state, LISP_PERF_CLEAR, off, 450));
    assert(state.metrics[LISP_PERF_CLEAR].count == 0);
    state.metrics[LISP_PERF_CLEAR].total_us = UINT64_MAX - 1;
    assert(lisp_perf_stats_record(&state, LISP_PERF_CLEAR, 2));
    assert(state.metrics[LISP_PERF_CLEAR].total_us == UINT64_MAX);
    state.metrics[LISP_PERF_CLEAR].count = UINT32_MAX;
    assert(!lisp_perf_stats_record(&state, LISP_PERF_CLEAR, 1));
    assert(lisp_perf_stats_stop(&state, &snapshot));
    state.generation = UINT32_MAX;
    assert(!lisp_perf_stats_start(&state));
    puts("PASS");
    return 0;
}
