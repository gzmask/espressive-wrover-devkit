#include "display_pipeline.h"
#include "display_probe_logic.h"
#include "lcd_terminal.h"
#include "lisp_app.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    STRIP_BYTES = DISPLAY_WIDTH * DISPLAY_STRIP_ROWS * 2,
    FRAME_BYTES = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2
};
static uint8_t strips[2][STRIP_BYTES], expected[FRAME_BYTES], actual[FRAME_BYTES];
static uint16_t pixels[LISP_APP_PIXELS];

typedef enum { APP, TERMINAL, PROBE } render_mode_t;
typedef struct {
    render_mode_t mode;
    lcd_terminal_t terminal;
    display_probe_view_t probe;
    const uint8_t *pending;
    unsigned pending_row;
    unsigned submits, waits;
} fixture_t;
static bool render_strip(fixture_t *f, uint8_t *buffer, unsigned row, unsigned rows)
{
    switch (f->mode) {
    case APP:
        return lisp_app_render_strip(pixels, buffer, STRIP_BYTES, row, rows);
    case TERMINAL:
        return lcd_terminal_render_strip(&f->terminal, buffer, STRIP_BYTES, row, rows);
    case PROBE:
        return display_probe_render_strip(buffer, STRIP_BYTES, row, rows, &f->probe);
    }
    return false;
}
static int render(void *user, uint8_t *buffer, unsigned row, unsigned rows)
{
    fixture_t *f = user;
    assert(buffer != f->pending);
    return render_strip(f, buffer, row, rows) ? 0 : 1;
}
static int submit(void *user, const uint8_t *buffer, unsigned row, unsigned rows)
{
    fixture_t *f = user;
    assert(!f->pending && rows == DISPLAY_STRIP_ROWS && row == f->submits * DISPLAY_STRIP_ROWS);
    f->pending = buffer;
    f->pending_row = row;
    f->submits++;
    return 0;
}
static int wait_done(void *user)
{
    fixture_t *f = user;
    assert(f->pending);
    memcpy(actual + (size_t)f->pending_row * DISPLAY_WIDTH * 2, f->pending, STRIP_BYTES);
    f->pending = NULL;
    f->waits++;
    return 0;
}
int main(void)
{
    for (size_t i = 0; i < LISP_APP_PIXELS; i++)
        pixels[i] = (uint16_t)((i * 71U + i / 13U) & 0xffffU);
    for (int mode = APP; mode <= PROBE; mode++) {
        fixture_t f = {.mode = (render_mode_t)mode,
                       .probe = {.model = DISPLAY_ILI9341,
                                 .id_match = true,
                                 .uptime_s = 123,
                                 .free_heap_bytes = 98765,
                                 .firmware = "test",
                                 .wifi = wifi_scan_initial(),
                                 .link = wifi_link_initial()}};
        lcd_terminal_init(&f.terminal);
        const char *message = "Pipeline test\nRGB565";
        lcd_terminal_write(&f.terminal, message, strlen(message));
        lcd_terminal_input(&f.terminal, "lbm> ", "(+ 1 2)", 7);
        for (unsigned row = 0; row < DISPLAY_HEIGHT; row += DISPLAY_STRIP_ROWS)
            assert(render_strip(&f, expected + (size_t)row * DISPLAY_WIDTH * 2, row, DISPLAY_STRIP_ROWS));
        memset(actual, 0xa5, sizeof(actual));
        const display_pipeline_ops_t ops = {&f, render, submit, wait_done};
        display_pipeline_result_t result =
            display_pipeline_run(&ops, strips[0], strips[1], STRIP_BYTES, DISPLAY_HEIGHT, DISPLAY_STRIP_ROWS);
        assert(result.status == DISPLAY_PIPELINE_OK && !result.dma_uncertain);
        assert(f.submits == DISPLAY_HEIGHT / DISPLAY_STRIP_ROWS && f.waits == f.submits && !f.pending);
        assert(memcmp(actual, expected, FRAME_BYTES) == 0);
    }
    puts("PASS: production RGB565 strip renderers reproduce sequential frame bytes");
    return 0;
}
