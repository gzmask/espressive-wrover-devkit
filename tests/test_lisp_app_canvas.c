#include "lcd_terminal.h"
#include "lisp_app.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t pixels[2][LISP_APP_PIXELS];
static lisp_app_canvas_t canvas;
static uint16_t actual[LISP_APP_PIXELS + 4], expected[LISP_APP_PIXELS + 4];
static uint8_t rendered[LISP_APP_PIXELS * 2 + 8], expected_strip[LISP_APP_PIXELS * 2 + 8];

static void check_rect(int x, int y, int width, int height, uint16_t color, size_t offset)
{
    for (size_t i = 0; i < LISP_APP_PIXELS + 4; i++)
        actual[i] = expected[i] = (uint16_t)(i * 31U);
    if (width > 0 && height > 0) {
        int64_t right = (int64_t)x + width, bottom = (int64_t)y + height;
        for (int row = 0; row < LISP_APP_HEIGHT; row++)
            for (int col = 0; col < LISP_APP_WIDTH; col++)
                if ((int64_t)col >= x && (int64_t)col < right && (int64_t)row >= y && (int64_t)row < bottom)
                    expected[offset + (size_t)row * LISP_APP_WIDTH + (unsigned)col] = color;
    }
    lisp_app_rect(actual + offset, x, y, width, height, color);
    assert(!memcmp(actual, expected, sizeof(actual)));
}

static void raster_boundaries(void)
{
    const uint16_t colors[] = {0, 2, 0x1234, 0xffff, 0xaaaa};
    lisp_app_clear(NULL, 1);
    lisp_app_rect(NULL, 0, 0, 10, 10, 1);
    for (size_t offset = 1; offset <= 2; offset++) {
        for (size_t c = 0; c < sizeof(colors) / sizeof(colors[0]); c++) {
            for (size_t i = 0; i < LISP_APP_PIXELS + 4; i++)
                actual[i] = expected[i] = (uint16_t)(i * 31U);
            for (size_t i = 0; i < LISP_APP_PIXELS; i++)
                expected[offset + i] = colors[c];
            lisp_app_clear(actual + offset, colors[c]);
            assert(!memcmp(actual, expected, sizeof(actual)));
        }
        check_rect(INT_MIN, INT_MIN, INT_MAX, INT_MAX, 1, offset);
        check_rect(INT_MAX, INT_MAX, INT_MAX, INT_MAX, 2, offset);
        check_rect(-10, -10, INT_MAX, INT_MAX, 0x1234, offset);
        check_rect(-2, -3, 7, 9, 2, offset);
        check_rect(319, 239, 20, 20, 0xffff, offset);
        check_rect(321, 0, 1, 1, 4, offset);
        check_rect(0, 241, 1, 1, 4, offset);
        check_rect(0, 0, 0, 100, 4, offset);
        check_rect(0, 0, 100, -1, 4, offset);
        for (int i = 0; i < 160; i++)
            check_rect((i * 61) % 480 - 80, (i * 97) % 360 - 60, (i * 37) % 180 - 10, (i * 43) % 140 - 10,
                       (uint16_t)(i * 379), offset);
    }
}
/* The original per-pixel implementation is also the overlap reference. */
static void reference_strip(const uint16_t *source, uint8_t *out, unsigned first, unsigned rows)
{
    size_t offset = (size_t)first * LISP_APP_WIDTH;
    for (size_t i = 0; i < (size_t)rows * LISP_APP_WIDTH; i++) {
        uint16_t color = source[offset + i];
        out[2 * i] = (uint8_t)(color >> 8);
        out[2 * i + 1] = (uint8_t)color;
    }
}
static void strip_boundaries(void)
{
    const unsigned firsts[] = {0, 1, 17, 224, 239};
    const unsigned rows[] = {1, 3, 16, 240};
    for (size_t offset = 1; offset <= 2; offset++) {
        for (size_t i = 0; i < LISP_APP_PIXELS + 4; i++)
            actual[i] = expected[i] = (uint16_t)(i * 227U + 0x1234U);
        for (size_t a = 0; a < sizeof(firsts) / sizeof(firsts[0]); a++) {
            for (size_t b = 0; b < sizeof(rows) / sizeof(rows[0]); b++) {
                if (rows[b] > LISP_APP_HEIGHT - firsts[a])
                    continue;
                size_t bytes = (size_t)rows[b] * LISP_APP_WIDTH * 2;
                for (size_t out_offset = 0; out_offset < 4; out_offset++) {
                    memset(rendered, 0xa5, sizeof(rendered));
                    memset(expected_strip, 0xa5, sizeof(expected_strip));
                    reference_strip(expected + offset, expected_strip + out_offset, firsts[a], rows[b]);
                    assert(lisp_app_render_strip(actual + offset, rendered + out_offset, bytes, firsts[a],
                                                 rows[b]));
                    assert(!memcmp(rendered, expected_strip, sizeof(rendered)));
                    assert(!memcmp(actual, expected, sizeof(actual)));
                }
            }
        }
        for (size_t shift = 0; shift <= 4; shift++) {
            for (size_t i = 0; i < LISP_APP_PIXELS + 4; i++)
                actual[i] = expected[i] = (uint16_t)(i * 227U + 0x1234U);
            reference_strip(expected + offset, (uint8_t *)(expected + offset) + shift, 0, 1);
            assert(lisp_app_render_strip(actual + offset, (uint8_t *)(actual + offset) + shift,
                                         LISP_APP_WIDTH * 2, 0, 1));
            assert(!memcmp(actual, expected, sizeof(actual)));
        }
    }
    memset(rendered, 0xa5, sizeof(rendered));
    memcpy(expected_strip, rendered, sizeof(rendered));
    assert(!lisp_app_render_strip(NULL, rendered, sizeof(rendered), 0, 1));
    assert(!lisp_app_render_strip(actual, NULL, sizeof(rendered), 0, 1));
    assert(!lisp_app_render_strip(actual, rendered, sizeof(rendered), UINT_MAX, 1));
    assert(!lisp_app_render_strip(actual, rendered, sizeof(rendered), 0, UINT_MAX));
    assert(!lisp_app_render_strip(actual, rendered, sizeof(rendered), 239, 2));
    assert(!lisp_app_render_strip(actual, rendered, 639, 0, 1));
    assert(!lisp_app_render_strip(actual, rendered, sizeof(rendered), 0, 0));
    assert(!memcmp(rendered, expected_strip, sizeof(rendered)));
}
static void init(void)
{
    assert(!lisp_app_canvas_init(NULL, pixels[0], pixels[1], LISP_APP_PIXELS));
    assert(!lisp_app_canvas_init(&canvas, pixels[0], pixels[0], LISP_APP_PIXELS));
    assert(!lisp_app_canvas_init(&canvas, pixels[0], pixels[1], LISP_APP_PIXELS - 1));
    assert(lisp_app_canvas_init(&canvas, pixels[0], pixels[1], LISP_APP_PIXELS));
}
static void terminal_frame(void)
{
    lisp_app_frame_t frame;
    assert(lisp_app_canvas_acquire(&canvas, &frame));
    assert(frame.pixels == NULL);
    lisp_app_canvas_complete(&canvas, &frame, true);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    init();
    if (!strcmp(argv[1], "switch")) {
        lcd_terminal_t terminal, saved;
        lcd_terminal_init(&terminal);
        lcd_terminal_write(&terminal, "keep this terminal", 18);
        saved = terminal;
        assert(lisp_app_canvas_begin(&canvas));
        assert(!lisp_app_canvas_begin(&canvas));
        uint16_t *draw = lisp_app_canvas_draw(&canvas);
        assert(draw);
        lisp_app_clear(draw, 0xf800);
        assert(lisp_app_canvas_publish(&canvas));
        terminal_frame(); /* Init cannot take the screen before activation. */
        lisp_app_canvas_activate(&canvas);
        lisp_app_frame_t frame;
        assert(lisp_app_canvas_acquire(&canvas, &frame));
        assert(frame.pixels && frame.pixels[0] == 0xf800);
        lisp_app_canvas_complete(&canvas, &frame, true);
        lisp_app_canvas_restore(&canvas);
        assert(lisp_app_canvas_state(&canvas) == LISP_APP_DISPLAY_WAIT);
        terminal_frame();
        assert(lisp_app_canvas_state(&canvas) == LISP_APP_DISPLAY_TERMINAL);
        assert(!memcmp(&saved, &terminal, sizeof(saved)));
    } else if (!strcmp(argv[1], "ownership")) {
        assert(lisp_app_canvas_begin(&canvas));
        lisp_app_clear(lisp_app_canvas_draw(&canvas), 11);
        assert(lisp_app_canvas_publish(&canvas));
        assert(lisp_app_canvas_draw(&canvas) == NULL);
        assert(!lisp_app_canvas_publish(&canvas));
        lisp_app_canvas_activate(&canvas);
        lisp_app_frame_t a, b;
        assert(lisp_app_canvas_acquire(&canvas, &a));
        assert(!lisp_app_canvas_acquire(&canvas, &b));
        uint16_t *draw = lisp_app_canvas_draw(&canvas);
        assert(draw && draw != a.pixels && draw[0] == 11);
        draw[0] = 22;
        assert(a.pixels[0] == 11);
        assert(lisp_app_canvas_publish(&canvas));
        assert(lisp_app_canvas_draw(&canvas) == NULL);
        lisp_app_canvas_complete(&canvas, &a, true);
        assert(lisp_app_canvas_acquire(&canvas, &b));
        assert(b.pixels[0] == 22);
        lisp_app_canvas_restore(&canvas);
        assert(!lisp_app_canvas_begin(&canvas)); /* Old front remains pinned. */
        assert(b.pixels[0] == 22);
        lisp_app_canvas_complete(&canvas, &b, true);
        assert(!lisp_app_canvas_begin(&canvas)); /* Need a confirmed terminal frame. */
        terminal_frame();
        assert(lisp_app_canvas_begin(&canvas));
    } else if (!strcmp(argv[1], "stale")) {
        lisp_app_frame_t frame;
        assert(lisp_app_canvas_acquire(&canvas, &frame));
        assert(lisp_app_canvas_begin(&canvas));
        lisp_app_canvas_restore(&canvas);
        lisp_app_canvas_complete(&canvas, &frame, true);
        assert(lisp_app_canvas_state(&canvas) == LISP_APP_DISPLAY_WAIT);
        terminal_frame();
        assert(lisp_app_canvas_state(&canvas) == LISP_APP_DISPLAY_TERMINAL);
    } else if (!strcmp(argv[1], "fault")) {
        assert(lisp_app_canvas_begin(&canvas));
        lisp_app_clear(lisp_app_canvas_draw(&canvas), 7);
        assert(lisp_app_canvas_publish(&canvas));
        lisp_app_canvas_activate(&canvas);
        lisp_app_frame_t frame;
        assert(lisp_app_canvas_acquire(&canvas, &frame));
        lisp_app_canvas_complete(&canvas, &frame, false);
        lisp_app_canvas_restore(&canvas);
        assert(lisp_app_canvas_state(&canvas) == LISP_APP_DISPLAY_FAULT);
        assert(!lisp_app_canvas_begin(&canvas));
        assert(lisp_app_canvas_draw(&canvas) == NULL);
        assert(frame.pixels[0] == 7);
    } else if (!strcmp(argv[1], "raster-boundaries")) {
        raster_boundaries();
    } else if (!strcmp(argv[1], "strip-boundaries")) {
        strip_boundaries();
    } else if (!strcmp(argv[1], "raster")) {
        uint16_t *draw = pixels[0];
        lisp_app_clear(draw, 0);
        lisp_app_rect(draw, -5, -5, 8, 8, 0xf800);
        assert(draw[0] == 0xf800 && draw[2 * 320 + 2] == 0xf800 && draw[3] == 0);
        lisp_app_line(draw, -1000000, 100, 1000000, 100, 0x07e0);
        assert(draw[100 * 320] == 0x07e0 && draw[100 * 320 + 319] == 0x07e0);
        lisp_app_line(draw, 319, 239, 0, 0, 0xffff);
        assert(draw[0] == 0xffff && draw[LISP_APP_PIXELS - 1] == 0xffff);
        lisp_app_pixel(draw, -1, 0, 123);
        lisp_app_rect(draw, 2147483647, 0, 2147483647, 2147483647, 1);
        lisp_app_rect(draw, 0, 0, -1, 5, 1);
        lisp_app_text(draw, 10, 10, "Hello", 5, 0x001f);
        bool ink = false;
        for (unsigned y = 10; y < 22; y++)
            for (unsigned x = 10; x < 50; x++)
                ink |= draw[y * 320 + x] == 0x001f;
        assert(ink);
        uint8_t strip[320 * 2];
        assert(lisp_app_render_strip(draw, strip, sizeof(strip), 0, 1));
        assert(strip[0] == 0xff && strip[1] == 0xff);
        assert(!lisp_app_render_strip(draw, strip, sizeof(strip) - 1, 0, 1));
        assert(!lisp_app_render_strip(draw, strip, sizeof(strip), 240, 1));
    } else if (!strcmp(argv[1], "controls")) {
        uint8_t key = 0;
        assert(lisp_app_parse_control(":stop", 5, &key) == LISP_APP_CONTROL_STOP);
        assert(lisp_app_parse_control(":key q", 6, &key) == LISP_APP_CONTROL_KEY && key == 'q');
        assert(lisp_app_parse_control(":key ", 5, &key) == LISP_APP_CONTROL_INVALID);
        assert(lisp_app_parse_control(":key qq", 7, &key) == LISP_APP_CONTROL_INVALID);
        assert(lisp_app_parse_control("(+ 1 2)", 7, &key) == LISP_APP_CONTROL_NONE);
        assert(lisp_app_parse_control(NULL, 0, &key) == LISP_APP_CONTROL_NONE);
        assert(lisp_app_parse_control(":stopx", 6, &key) == LISP_APP_CONTROL_NONE);
    } else
        return 1;
    puts("PASS");
    return 0;
}
