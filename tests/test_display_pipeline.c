#include "display_pipeline.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { STRIP_BYTES = 16, MAX_STRIPS = 16, ERR_RENDER = 11, ERR_SUBMIT = 12, ERR_WAIT = 13 };
typedef struct {
    uint8_t *buffers[2];
    const uint8_t *pending;
    uint8_t expected[STRIP_BYTES];
    unsigned pending_row;
    unsigned rendered, submitted, waited;
    unsigned fail_render, fail_submit, fail_wait;
    unsigned order[MAX_STRIPS];
    bool simulate_immediate_completion;
    bool completed;
} fixture_t;

static void init(fixture_t *f, uint8_t *a, uint8_t *b)
{
    memset(f, 0, sizeof(*f));
    f->buffers[0] = a;
    f->buffers[1] = b;
    f->fail_render = f->fail_submit = f->fail_wait = UINT_MAX;
}
static int render(void *user, uint8_t *buffer, unsigned row, unsigned rows)
{
    fixture_t *f = user;
    assert(rows == 1);
    assert(buffer == f->buffers[f->rendered % 2]);
    if (f->pending) {
        assert(buffer != f->pending);
        assert(memcmp(f->pending, f->expected, STRIP_BYTES) == 0);
    }
    unsigned index = f->rendered++;
    if (index == f->fail_render)
        return ERR_RENDER;
    memset(buffer, (int)row, STRIP_BYTES);
    return 0;
}
static int submit(void *user, const uint8_t *buffer, unsigned row, unsigned rows)
{
    fixture_t *f = user;
    assert(rows == 1 && !f->pending);
    assert(buffer == f->buffers[f->submitted % 2]);
    assert(f->rendered == f->submitted + 1);
    unsigned index = f->submitted++;
    f->pending = buffer;
    f->pending_row = row;
    memcpy(f->expected, buffer, STRIP_BYTES);
    f->completed = f->simulate_immediate_completion;
    if (index == f->fail_submit)
        return ERR_SUBMIT; /* It may have queued DMA despite the error. */
    return 0;
}
static int wait_done(void *user)
{
    fixture_t *f = user;
    assert(f->pending);
    assert(memcmp(f->pending, f->expected, STRIP_BYTES) == 0);
    unsigned index = f->waited++;
    if (index == f->fail_wait)
        return ERR_WAIT;
    f->completed = true;
    f->order[index] = f->pending_row;
    f->pending = NULL;
    return 0;
}
static display_pipeline_result_t run(fixture_t *f, unsigned height)
{
    const display_pipeline_ops_t ops = {f, render, submit, wait_done};
    return display_pipeline_run(&ops, f->buffers[0], f->buffers[1], STRIP_BYTES, height, 1);
}
static void success(void)
{
    uint8_t a[STRIP_BYTES], b[STRIP_BYTES];
    for (unsigned immediate = 0; immediate < 2; immediate++) {
        for (unsigned height = 1; height <= MAX_STRIPS; height++) {
            fixture_t f;
            init(&f, a, b);
            f.simulate_immediate_completion = immediate != 0;
            display_pipeline_result_t result = run(&f, height);
            assert(result.status == DISPLAY_PIPELINE_OK && !result.dma_uncertain);
            assert(result.error == 0 && !f.pending && f.completed);
            assert(f.rendered == height && f.submitted == height && f.waited == height);
            for (unsigned i = 0; i < height; i++)
                assert(f.order[i] == i);
        }
    }
}
static void faults(void)
{
    uint8_t a[STRIP_BYTES], b[STRIP_BYTES];
    for (unsigned fail = 0; fail < MAX_STRIPS; fail++) {
        fixture_t f;
        init(&f, a, b);
        f.fail_render = fail;
        display_pipeline_result_t result = run(&f, MAX_STRIPS);
        assert(result.status == DISPLAY_PIPELINE_RENDER_ERROR && result.error == ERR_RENDER);
        assert(result.dma_uncertain == (fail != 0));
        assert(f.rendered == fail + 1 && f.submitted == fail && f.waited == (fail ? fail - 1 : 0));
        assert((f.pending != NULL) == (fail != 0));

        init(&f, a, b);
        f.fail_submit = fail;
        result = run(&f, MAX_STRIPS);
        assert(result.status == DISPLAY_PIPELINE_SUBMIT_ERROR && result.error == ERR_SUBMIT);
        assert(result.dma_uncertain && f.pending);
        assert(f.rendered == fail + 1 && f.submitted == fail + 1 && f.waited == fail);

        init(&f, a, b);
        f.fail_wait = fail;
        result = run(&f, MAX_STRIPS);
        assert(result.status == DISPLAY_PIPELINE_WAIT_ERROR && result.error == ERR_WAIT);
        assert(result.dma_uncertain && f.pending);
        assert(f.submitted == fail + 1 && f.waited == fail + 1);
        assert(f.rendered == (fail + 1 < MAX_STRIPS ? fail + 2 : MAX_STRIPS));
    }
}
static void invalid(void)
{
    uint8_t memory[STRIP_BYTES * 3];
    fixture_t f;
    init(&f, memory, memory + STRIP_BYTES);
    display_pipeline_ops_t ops = {&f, render, submit, wait_done};
    display_pipeline_result_t result;
#define INVALID(...)                                                                                         \
    do {                                                                                                     \
        result = display_pipeline_run(__VA_ARGS__);                                                          \
        assert(result.status == DISPLAY_PIPELINE_INVALID && !result.dma_uncertain);                          \
        assert(f.rendered == 0 && f.submitted == 0 && f.waited == 0);                                        \
    } while (0)
    INVALID(NULL, memory, memory + STRIP_BYTES, STRIP_BYTES, 1, 1);
    INVALID(&ops, NULL, memory + STRIP_BYTES, STRIP_BYTES, 1, 1);
    INVALID(&ops, memory, memory, STRIP_BYTES, 1, 1);
    INVALID(&ops, memory, memory + 1, STRIP_BYTES, 1, 1);
    INVALID(&ops, memory, memory + STRIP_BYTES - 1, STRIP_BYTES, 1, 1);
    INVALID(&ops, memory + STRIP_BYTES - 1, memory, STRIP_BYTES, 1, 1);
    INVALID(&ops, memory, memory + STRIP_BYTES, 0, 1, 1);
    INVALID(&ops, memory, memory + STRIP_BYTES, STRIP_BYTES, 0, 1);
    INVALID(&ops, memory, memory + STRIP_BYTES, STRIP_BYTES, 1, 0);
    INVALID(&ops, memory, memory + STRIP_BYTES, STRIP_BYTES, 3, 2);
    ops.render = NULL;
    INVALID(&ops, memory, memory + STRIP_BYTES, STRIP_BYTES, 1, 1);
    ops.render = render;
    ops.submit = NULL;
    INVALID(&ops, memory, memory + STRIP_BYTES, STRIP_BYTES, 1, 1);
    ops.submit = submit;
    ops.wait = NULL;
    INVALID(&ops, memory, memory + STRIP_BYTES, STRIP_BYTES, 1, 1);
#undef INVALID
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "success"))
        success();
    else if (!strcmp(argv[1], "faults"))
        faults();
    else if (!strcmp(argv[1], "invalid"))
        invalid();
    else
        return 2;
    puts("PASS");
    return 0;
}
