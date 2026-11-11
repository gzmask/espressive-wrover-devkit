#include "display_pipeline.h"
#include "lcd_terminal.h"
#include "lisp_app.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { STRIP_BYTES = LISP_APP_WIDTH * 16 * 2 };
static uint16_t pixels[2][LISP_APP_PIXELS];
static uint8_t strips[2][STRIP_BYTES];
typedef struct {
    lisp_app_canvas_t canvas;
    lisp_app_frame_t frame;
    lcd_terminal_t terminal;
    const uint8_t *pending;
    uint8_t expected[STRIP_BYTES];
    unsigned renders, submits, waits;
    bool restore, fail_render, fail_submit, fail_wait;
} fixture_t;

static int render(void *user, uint8_t *buffer, unsigned row, unsigned rows)
{
    fixture_t *f = user;
    assert(f->canvas.frame_open && buffer != f->pending);
    if (f->pending)
        assert(memcmp(f->pending, f->expected, STRIP_BYTES) == 0);
    f->renders++;
    if (f->frame.pixels) {
        assert(f->canvas.reading && f->frame.pixels[0] == 0x07e0);
        if (row == 0) {
            /* Owner may publish the OTHER full canvas while LCD reads this frame. */
            uint16_t *draw = lisp_app_canvas_draw(&f->canvas);
            assert(draw && draw != f->frame.pixels);
            lisp_app_clear(draw, 0x001f);
            assert(lisp_app_canvas_publish(&f->canvas));
        }
        if (row == 16 && f->restore)
            lisp_app_canvas_restore(&f->canvas);
        assert(!lisp_app_canvas_begin(&f->canvas));
    } else {
        assert(!f->canvas.terminal_confirmed);
    }
    if (row == 16 && f->fail_render)
        return 1;
    bool ok = f->frame.pixels ? lisp_app_render_strip(f->frame.pixels, buffer, STRIP_BYTES, row, rows)
                              : lcd_terminal_render_strip(&f->terminal, buffer, STRIP_BYTES, row, rows);
    return ok ? 0 : 1;
}
static int submit(void *user, const uint8_t *buffer, unsigned row, unsigned rows)
{
    fixture_t *f = user;
    assert(!f->pending && row == f->submits * 16 && rows == 16 && f->canvas.frame_open);
    f->submits++;
    f->pending = buffer;
    memcpy(f->expected, buffer, STRIP_BYTES);
    return f->fail_submit ? 2 : 0;
}
static int wait_done(void *user)
{
    fixture_t *f = user;
    assert(f->canvas.frame_open && f->pending);
    assert(memcmp(f->pending, f->expected, STRIP_BYTES) == 0);
    f->waits++;
    if (f->fail_wait)
        return 3;
    f->pending = NULL;
    return 0;
}
static void init(fixture_t *f)
{
    memset(f, 0, sizeof(*f));
    lcd_terminal_init(&f->terminal);
    assert(lisp_app_canvas_init(&f->canvas, pixels[0], pixels[1], LISP_APP_PIXELS));
    assert(lisp_app_canvas_begin(&f->canvas));
    lisp_app_clear(lisp_app_canvas_draw(&f->canvas), 0x07e0);
    assert(lisp_app_canvas_publish(&f->canvas));
    lisp_app_canvas_activate(&f->canvas);
    assert(lisp_app_canvas_acquire(&f->canvas, &f->frame));
    assert(f->frame.pixels && f->canvas.reading && f->canvas.frame_open);
}
static display_pipeline_result_t run(fixture_t *f)
{
    const display_pipeline_ops_t ops = {f, render, submit, wait_done};
    return display_pipeline_run(&ops, strips[0], strips[1], STRIP_BYTES, LISP_APP_HEIGHT, 16);
}
static void ownership(void)
{
    fixture_t f;
    init(&f);
    display_pipeline_result_t result = run(&f);
    assert(result.status == DISPLAY_PIPELINE_OK && !f.pending && f.waits == 15);
    assert(f.canvas.frame_open && f.canvas.reading);
    lisp_app_canvas_complete(&f.canvas, &f.frame, true);
    assert(!f.canvas.frame_open && !f.canvas.reading && !f.canvas.fault);
    assert(lisp_app_canvas_acquire(&f.canvas, &f.frame));
    assert(f.frame.pixels[0] == 0x001f); /* Next published frame was not substituted mid-frame. */
    lisp_app_canvas_complete(&f.canvas, &f.frame, true);
}
static void restore(void)
{
    fixture_t f;
    init(&f);
    f.restore = true;
    display_pipeline_result_t result = run(&f);
    assert(result.status == DISPLAY_PIPELINE_OK && !f.pending && f.waits == 15);
    lisp_app_canvas_complete(&f.canvas, &f.frame, true);
    assert(!f.canvas.terminal_confirmed); /* Finishing the old app is not a terminal acknowledgement. */
    assert(lisp_app_canvas_acquire(&f.canvas, &f.frame) && !f.frame.pixels);
    f.renders = f.submits = f.waits = 0;
    result = run(&f);
    assert(result.status == DISPLAY_PIPELINE_OK && !f.pending && f.waits == 15);
    assert(!f.canvas.terminal_confirmed);
    lisp_app_canvas_complete(&f.canvas, &f.frame, true);
    assert(f.canvas.terminal_confirmed && lisp_app_canvas_begin(&f.canvas));
}
static void faults(void)
{
    for (unsigned failure = 0; failure < 3; failure++) {
        fixture_t f;
        init(&f);
        f.fail_render = failure == 0;
        f.fail_submit = failure == 1;
        f.fail_wait = failure == 2;
        display_pipeline_result_t result = run(&f);
        assert(result.status != DISPLAY_PIPELINE_OK && result.dma_uncertain && f.pending);
        assert(f.submits == 1 && f.renders <= 2 && f.waits <= 1);
        lisp_app_canvas_complete(&f.canvas, &f.frame, false);
        assert(f.canvas.fault && f.canvas.frame_open && f.canvas.reading);
        assert(!lisp_app_canvas_begin(&f.canvas));
        assert(!lisp_app_canvas_draw(&f.canvas));
        lisp_app_frame_t other;
        assert(!lisp_app_canvas_acquire(&f.canvas, &other));
        /* Late DMA still sees retained bytes, even after failure was reported. */
        assert(memcmp(f.pending, f.expected, STRIP_BYTES) == 0 && f.frame.pixels[0] == 0x07e0);
    }
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "ownership"))
        ownership();
    else if (!strcmp(argv[1], "restore"))
        restore();
    else if (!strcmp(argv[1], "faults"))
        faults();
    else
        return 2;
    puts("PASS: production canvas ownership and completion, host DMA model only");
    return 0;
}
