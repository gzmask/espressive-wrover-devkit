#include "display_pipeline.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { STRIP_BYTES = 16, STRIP_ROWS = 16, FRAME_STRIPS = 15 };
typedef struct {
    uint64_t now_us, render_us, dma_us, completion_us, submit_us;
    uint64_t render_total_us, observed_transfer_us;
    const uint8_t *pending;
    uint8_t saved[STRIP_BYTES];
    unsigned rendered, submitted, completed;
} fixture_t;

static int render(void *user, uint8_t *buffer, unsigned row, unsigned rows)
{
    fixture_t *f = user;
    assert(rows == STRIP_ROWS && row == f->rendered * STRIP_ROWS && buffer != f->pending);
    if (f->pending)
        assert(memcmp(f->pending, f->saved, STRIP_BYTES) == 0);
    f->now_us += f->render_us;
    f->render_total_us += f->render_us;
    memset(buffer, (int)f->rendered, STRIP_BYTES);
    f->rendered++;
    return 0;
}
static int submit(void *user, const uint8_t *buffer, unsigned row, unsigned rows)
{
    fixture_t *f = user;
    assert(!f->pending && row == f->submitted * STRIP_ROWS && rows == STRIP_ROWS);
    f->pending = buffer;
    memcpy(f->saved, buffer, STRIP_BYTES);
    f->submit_us = f->now_us;
    f->completion_us = f->now_us + f->dma_us;
    f->submitted++;
    return 0;
}
static int wait_done(void *user)
{
    fixture_t *f = user;
    assert(f->pending && memcmp(f->pending, f->saved, STRIP_BYTES) == 0);
    if (f->now_us < f->completion_us)
        f->now_us = f->completion_us;
    f->observed_transfer_us += f->now_us - f->submit_us;
    f->pending = NULL;
    f->completed++;
    return 0;
}
int main(void)
{
    static const uint64_t render_costs[] = {0, 1, 3000, 10000, 30000};
    static const uint64_t dma_costs[] = {0, 1, 8192, 10000, 30000};
    uint8_t strips[2][STRIP_BYTES];
    for (size_t r = 0; r < sizeof(render_costs) / sizeof(render_costs[0]); r++) {
        for (size_t d = 0; d < sizeof(dma_costs) / sizeof(dma_costs[0]); d++) {
            for (unsigned count = 1; count <= FRAME_STRIPS; count++) {
                fixture_t f = {.render_us = render_costs[r], .dma_us = dma_costs[d]};
                const display_pipeline_ops_t ops = {&f, render, submit, wait_done};
                display_pipeline_result_t result = display_pipeline_run(
                    &ops, strips[0], strips[1], STRIP_BYTES, count * STRIP_ROWS, STRIP_ROWS);
                uint64_t overlap = f.render_us > f.dma_us ? f.render_us : f.dma_us;
                uint64_t expected = f.render_us + f.dma_us + (count - 1) * overlap;
                uint64_t sequential = count * (f.render_us + f.dma_us);
                assert(result.status == DISPLAY_PIPELINE_OK && !result.dma_uncertain);
                assert(!f.pending && f.completed == count && f.submitted == count && f.rendered == count);
                assert(f.now_us == expected && f.now_us >= count * f.dma_us && f.now_us <= sequential);
                assert(f.render_total_us == count * f.render_us);
                assert(f.observed_transfer_us == f.dma_us + (count - 1) * overlap);
                if (count > 1 && f.render_us && f.dma_us) {
                    assert(f.now_us < sequential);
                    /* Transfer observations include overlapping rendering; do not add them. */
                    assert(f.render_total_us + f.observed_transfer_us > f.now_us);
                }
            }
        }
    }
    puts("PASS: synthetic clock validates overlap/final wait, not ESP32 performance");
    return 0;
}
