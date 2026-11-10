#include "lcd_terminal.h"
#include "lisp_app.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t pixels[2][LISP_APP_PIXELS];
static lisp_app_canvas_t canvas;
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
