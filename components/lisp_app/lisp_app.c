#include "lisp_app.h"
#include "lcd_terminal.h"
#include <string.h>

bool lisp_app_canvas_init(lisp_app_canvas_t *canvas, uint16_t *a, uint16_t *b, size_t capacity)
{
    size_t bytes = LISP_APP_PIXELS * sizeof(uint16_t);
    uintptr_t pa = (uintptr_t)a, pb = (uintptr_t)b;
    if (!canvas || !a || !b || capacity < LISP_APP_PIXELS || pa > UINTPTR_MAX - bytes ||
        pb > UINTPTR_MAX - bytes || (pa < pb + bytes && pb < pa + bytes))
        return false;
    memset(canvas, 0, sizeof(*canvas));
    canvas->pixels[0] = a;
    canvas->pixels[1] = b;
    canvas->front = 1;
    canvas->draw = 0;
    canvas->pending = -1;
    canvas->terminal_confirmed = true;
    return true;
}
lisp_app_display_state_t lisp_app_canvas_state(const lisp_app_canvas_t *canvas)
{
    if (!canvas || canvas->fault)
        return LISP_APP_DISPLAY_FAULT;
    if (!canvas->session)
        return canvas->terminal_confirmed ? LISP_APP_DISPLAY_TERMINAL : LISP_APP_DISPLAY_WAIT;
    return canvas->draw >= 0 && canvas->pending < 0 ? LISP_APP_DISPLAY_DRAWABLE : LISP_APP_DISPLAY_WAIT;
}
bool lisp_app_canvas_begin(lisp_app_canvas_t *canvas)
{
    if (lisp_app_canvas_state(canvas) != LISP_APP_DISPLAY_TERMINAL || canvas->reading)
        return false;
    canvas->session = true;
    canvas->enabled = false;
    canvas->front = 1;
    canvas->draw = 0;
    canvas->pending = -1;
    canvas->generation++;
    return true;
}
uint16_t *lisp_app_canvas_draw(lisp_app_canvas_t *canvas)
{
    if (lisp_app_canvas_state(canvas) != LISP_APP_DISPLAY_DRAWABLE)
        return NULL;
    uint16_t *draw = canvas->pixels[canvas->draw];
    if (canvas->enabled)
        memcpy(draw, canvas->pixels[canvas->front], LISP_APP_PIXELS * sizeof(uint16_t));
    else
        memset(draw, 0, LISP_APP_PIXELS * sizeof(uint16_t));
    return draw;
}
bool lisp_app_canvas_publish(lisp_app_canvas_t *canvas)
{
    if (lisp_app_canvas_state(canvas) != LISP_APP_DISPLAY_DRAWABLE)
        return false;
    canvas->pending = canvas->draw;
    canvas->draw = -1;
    return true;
}
void lisp_app_canvas_activate(lisp_app_canvas_t *canvas)
{
    if (canvas && canvas->session && !canvas->fault && canvas->pending >= 0) {
        canvas->enabled = true;
        canvas->generation++;
    }
}
void lisp_app_canvas_restore(lisp_app_canvas_t *canvas)
{
    if (!canvas)
        return;
    canvas->enabled = false;
    canvas->session = false;
    canvas->pending = -1;
    canvas->terminal_confirmed = false;
    canvas->generation++;
}
bool lisp_app_canvas_acquire(lisp_app_canvas_t *canvas, lisp_app_frame_t *frame)
{
    if (!canvas || !frame || canvas->fault || canvas->frame_open)
        return false;
    canvas->frame_open = true;
    if (canvas->enabled && canvas->pending >= 0) {
        canvas->draw = canvas->front;
        canvas->front = canvas->pending;
        canvas->pending = -1;
    }
    frame->pixels = canvas->enabled ? canvas->pixels[canvas->front] : NULL;
    frame->generation = canvas->generation;
    canvas->reading = frame->pixels != NULL;
    canvas->frame_pixels = frame->pixels;
    canvas->frame_generation = frame->generation;
    return true;
}
void lisp_app_canvas_complete(lisp_app_canvas_t *canvas, const lisp_app_frame_t *frame, bool success)
{
    if (!canvas || !frame || !canvas->frame_open || canvas->frame_generation != frame->generation ||
        canvas->frame_pixels != frame->pixels)
        return;
    if (!success) {
        canvas->fault = true; /* Keep the front pinned; never recycle uncertain DMA data. */
        return;
    }
    canvas->frame_open = false;
    canvas->reading = false;
    if (!frame->pixels && frame->generation == canvas->generation && !canvas->session)
        canvas->terminal_confirmed = true;
}

/* A valid uint16_t canvas may only be two-byte aligned. Peel one pixel before
 * writing aligned pairs; may_alias preserves the uint16_t storage aliasing rules. */
#if defined(__GNUC__) || defined(__clang__)
typedef uint32_t pixel_pair_t __attribute__((__may_alias__));
#endif
static void fill_pixels(uint16_t *pixels, size_t count, uint16_t color)
{
    if (count && ((uintptr_t)pixels & 3U)) {
        *pixels++ = color;
        count--;
    }
#if defined(__GNUC__) || defined(__clang__)
    uint32_t pair = (uint32_t)color | ((uint32_t)color << 16);
    pixel_pair_t *pairs = (pixel_pair_t *)(void *)pixels;
    while (count >= 2) {
        *pairs++ = pair;
        count -= 2;
    }
    pixels = (uint16_t *)(void *)pairs;
#endif
    while (count--)
        *pixels++ = color;
}
void lisp_app_clear(uint16_t *pixels, uint16_t color)
{
    if (pixels)
        fill_pixels(pixels, LISP_APP_PIXELS, color);
}
void lisp_app_pixel(uint16_t *pixels, int x, int y, uint16_t color)
{
    if (pixels && x >= 0 && x < LISP_APP_WIDTH && y >= 0 && y < LISP_APP_HEIGHT)
        pixels[(size_t)y * LISP_APP_WIDTH + (unsigned)x] = color;
}
void lisp_app_rect(uint16_t *pixels, int x, int y, int width, int height, uint16_t color)
{
    if (!pixels || width <= 0 || height <= 0)
        return;
    int64_t right = (int64_t)x + width, bottom = (int64_t)y + height;
    int left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    int r = right > LISP_APP_WIDTH ? LISP_APP_WIDTH : (int)right;
    int b = bottom > LISP_APP_HEIGHT ? LISP_APP_HEIGHT : (int)bottom;
    if (left >= r || top >= b)
        return;
    size_t row_width = (size_t)(r - left);
    for (int yy = top; yy < b; yy++)
        fill_pixels(pixels + (size_t)yy * LISP_APP_WIDTH + (unsigned)left, row_width, color);
}
static bool clip(double p, double q, double *lo, double *hi)
{
    if (p == 0)
        return q >= 0;
    double t = q / p;
    if (p < 0) {
        if (t > *hi)
            return false;
        if (t > *lo)
            *lo = t;
    } else {
        if (t < *lo)
            return false;
        if (t < *hi)
            *hi = t;
    }
    return true;
}
static int bounded(double coordinate, int maximum)
{
    return coordinate < 0 ? 0 : coordinate > maximum ? maximum : (int)(coordinate + 0.5);
}
void lisp_app_line(uint16_t *pixels, int x0, int y0, int x1, int y1, uint16_t color)
{
    if (!pixels)
        return;
    /* Clip before Bresenham: extreme inputs cannot cause billions of iterations
     * or overflow integer products. All raster loops are screen-bounded. */
    double dx = (double)x1 - x0, dy = (double)y1 - y0, lo = 0, hi = 1;
    if (!clip(-dx, x0, &lo, &hi) || !clip(dx, 319.0 - x0, &lo, &hi) || !clip(-dy, y0, &lo, &hi) ||
        !clip(dy, 239.0 - y0, &lo, &hi))
        return;
    x1 = bounded(x0 + hi * dx, 319);
    y1 = bounded(y0 + hi * dy, 239);
    x0 = bounded(x0 + lo * dx, 319);
    y0 = bounded(y0 + lo * dy, 239);
    int ax = x1 > x0 ? x1 - x0 : x0 - x1, ay = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, error = ax + ay;
    for (;;) {
        lisp_app_pixel(pixels, x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        int twice = 2 * error;
        if (twice >= ay) {
            error += ay;
            x0 += sx;
        }
        if (twice <= ax) {
            error += ax;
            y0 += sy;
        }
    }
}
void lisp_app_text(uint16_t *pixels, int x, int y, const char *text, size_t length, uint16_t color)
{
    if (!pixels || !text)
        return;
    if (length > 128)
        length = 128;
    for (size_t i = 0; i < length; i++) {
        int64_t left = (int64_t)x + (int64_t)i * LCD_TERMINAL_CELL_WIDTH;
        for (unsigned yy = 0; yy < LCD_TERMINAL_CELL_HEIGHT; yy++)
            for (unsigned xx = 0; xx < LCD_TERMINAL_CELL_WIDTH; xx++) {
                int64_t px = left + xx, py = (int64_t)y + yy;
                if (px >= 0 && px < LISP_APP_WIDTH && py >= 0 && py < LISP_APP_HEIGHT &&
                    lcd_terminal_glyph_pixel((unsigned char)text[i], xx, yy))
                    lisp_app_pixel(pixels, (int)px, (int)py, color);
            }
    }
}
bool lisp_app_render_strip(const uint16_t *pixels, uint8_t *out, size_t capacity, unsigned first_row,
                           unsigned rows)
{
    if (!pixels || !out || !rows || first_row >= LISP_APP_HEIGHT || rows > LISP_APP_HEIGHT - first_row ||
        capacity < (size_t)rows * LISP_APP_WIDTH * 2)
        return false;
    const uint16_t *source = pixels + (size_t)first_row * LISP_APP_WIDTH;
    size_t count = (size_t)rows * LISP_APP_WIDTH;
#if (defined(__GNUC__) || defined(__clang__)) && defined(__BYTE_ORDER__) &&                                  \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    /* The normal PSRAM canvas and DMA strip are aligned and disjoint. Preserve
     * the original byte-by-byte behavior for overlapping or unaligned buffers. */
    uintptr_t src_addr = (uintptr_t)source, out_addr = (uintptr_t)out;
    size_t bytes = count * sizeof(uint16_t);
    if (((src_addr ^ out_addr) & 3U) == 0 && src_addr <= UINTPTR_MAX - bytes &&
        out_addr <= UINTPTR_MAX - bytes && (src_addr + bytes <= out_addr || out_addr + bytes <= src_addr)) {
        if ((src_addr & 3U) && count) {
            uint16_t color = *source++;
            *out++ = (uint8_t)(color >> 8);
            *out++ = (uint8_t)color;
            count--;
        }
        const pixel_pair_t *src_pairs = (const pixel_pair_t *)(const void *)source;
        pixel_pair_t *out_pairs = (pixel_pair_t *)(void *)out;
        while (count >= 2) {
            uint32_t pair = *src_pairs++;
            *out_pairs++ = ((pair & 0x00ff00ffU) << 8) | ((pair & 0xff00ff00U) >> 8);
            count -= 2;
        }
        source = (const uint16_t *)(const void *)src_pairs;
        out = (uint8_t *)(void *)out_pairs;
    }
#endif
    while (count--) {
        uint16_t color = *source++;
        *out++ = (uint8_t)(color >> 8);
        *out++ = (uint8_t)color;
    }
    return true;
}
lisp_app_control_t lisp_app_parse_control(const char *source, size_t length, uint8_t *key)
{
    if (!source)
        return LISP_APP_CONTROL_NONE;
    if (length == 5 && !memcmp(source, ":stop", 5))
        return LISP_APP_CONTROL_STOP;
    if (length >= 4 && !memcmp(source, ":key", 4) && (length == 4 || source[4] == ' ')) {
        if (length != 6 || !key || (unsigned char)source[5] < 32 || (unsigned char)source[5] > 126)
            return LISP_APP_CONTROL_INVALID;
        *key = (uint8_t)source[5];
        return LISP_APP_CONTROL_KEY;
    }
    return LISP_APP_CONTROL_NONE;
}
