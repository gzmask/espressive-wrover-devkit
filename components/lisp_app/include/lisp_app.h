#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { LISP_APP_WIDTH = 320, LISP_APP_HEIGHT = 240, LISP_APP_PIXELS = 320 * 240 };
typedef enum {
    LISP_APP_DISPLAY_TERMINAL,
    LISP_APP_DISPLAY_DRAWABLE,
    LISP_APP_DISPLAY_WAIT,
    LISP_APP_DISPLAY_FAULT,
} lisp_app_display_state_t;
/* All metadata operations must be serialized by the caller. Only the Lisp
 * owner writes the draw slot. Pixel copies/rasterization never hold a lock.
 * Fixed buffers live until reboot, including uncertain DMA completion. */
typedef struct {
    uint16_t *pixels[2];
    int front, draw, pending;
    bool session, enabled, frame_open, reading, terminal_confirmed, fault;
    uint32_t generation, frame_generation;
    const uint16_t *frame_pixels;
} lisp_app_canvas_t;
typedef struct {
    const uint16_t *pixels; /* NULL selects the terminal; immutable until complete. */
    uint32_t generation;
} lisp_app_frame_t;
bool lisp_app_canvas_init(lisp_app_canvas_t *canvas, uint16_t *a, uint16_t *b, size_t capacity);
bool lisp_app_canvas_begin(lisp_app_canvas_t *canvas);
/* Copies the immutable front into the available back; caller holds NO lock. */
uint16_t *lisp_app_canvas_draw(lisp_app_canvas_t *canvas);
bool lisp_app_canvas_publish(lisp_app_canvas_t *canvas);
void lisp_app_canvas_activate(lisp_app_canvas_t *canvas);
void lisp_app_canvas_restore(lisp_app_canvas_t *canvas);
lisp_app_display_state_t lisp_app_canvas_state(const lisp_app_canvas_t *canvas);
bool lisp_app_canvas_acquire(lisp_app_canvas_t *canvas, lisp_app_frame_t *frame);
void lisp_app_canvas_complete(lisp_app_canvas_t *canvas, const lisp_app_frame_t *frame, bool success);

/* Owner backend. draw() copies pixels outside metadata locks. No Lisp values
 * or stack pointers cross to the LCD task. activate() follows successful init;
 * restoration is acknowledged only by a completed terminal frame. */
typedef struct {
    void *user;
    bool (*begin)(void *user);
    uint16_t *(*draw)(void *user);
    bool (*publish)(void *user);
    void (*activate)(void *user);
    void (*restore)(void *user);
    lisp_app_display_state_t (*state)(void *user);
} lisp_app_display_t;

void lisp_app_clear(uint16_t *pixels, uint16_t color);
void lisp_app_pixel(uint16_t *pixels, int x, int y, uint16_t color);
void lisp_app_line(uint16_t *pixels, int x0, int y0, int x1, int y1, uint16_t color);
void lisp_app_rect(uint16_t *pixels, int x, int y, int width, int height, uint16_t color);
void lisp_app_text(uint16_t *pixels, int x, int y, const char *text, size_t length, uint16_t color);
bool lisp_app_render_strip(const uint16_t *pixels, uint8_t *out, size_t capacity, unsigned first_row,
                           unsigned rows);
typedef enum {
    LISP_APP_CONTROL_NONE,
    LISP_APP_CONTROL_STOP,
    LISP_APP_CONTROL_KEY,
    LISP_APP_CONTROL_INVALID,
} lisp_app_control_t;
lisp_app_control_t lisp_app_parse_control(const char *source, size_t length, uint8_t *key);
