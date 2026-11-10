#define _POSIX_C_SOURCE 200809L
#include "lisp_app.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static lisp_app_canvas_t canvas;
static uint16_t pixels[2][LISP_APP_PIXELS];
static atomic_bool halt;
static atomic_uint app_frames;
static void pause_tick(void)
{
    struct timespec delay = {0, 100000};
    nanosleep(&delay, NULL);
}
static void *lcd(void *unused)
{
    (void)unused;
    while (!atomic_load(&halt)) {
        lisp_app_frame_t frame;
        pthread_mutex_lock(&lock);
        assert(lisp_app_canvas_acquire(&canvas, &frame));
        pthread_mutex_unlock(&lock);
        if (frame.pixels) {
            uint16_t expected = frame.pixels[0];
            uint8_t strip[640];
            for (unsigned y = 0; y < LISP_APP_HEIGHT; y++) {
                assert(lisp_app_render_strip(frame.pixels, strip, sizeof(strip), y, 1));
                for (unsigned x = 0; x < LISP_APP_WIDTH; x++)
                    assert(strip[2 * x] == (uint8_t)(expected >> 8) && strip[2 * x + 1] == (uint8_t)expected);
                if (y % 32 == 0)
                    sched_yield(); /* Owner may draw/publish/stop while front is read. */
            }
            atomic_fetch_add(&app_frames, 1);
        }
        pthread_mutex_lock(&lock);
        lisp_app_canvas_complete(&canvas, &frame, true);
        pthread_mutex_unlock(&lock);
        pause_tick();
    }
    return NULL;
}
static lisp_app_display_state_t state(void)
{
    pthread_mutex_lock(&lock);
    lisp_app_display_state_t result = lisp_app_canvas_state(&canvas);
    pthread_mutex_unlock(&lock);
    return result;
}
static uint16_t *draw(void)
{
    pthread_mutex_lock(&lock);
    lisp_app_canvas_t snapshot = canvas;
    pthread_mutex_unlock(&lock);
    return lisp_app_canvas_draw(&snapshot); /* Same out-of-lock copy as the device backend. */
}
int main(void)
{
    assert(lisp_app_canvas_init(&canvas, pixels[0], pixels[1], LISP_APP_PIXELS));
    pthread_t thread;
    assert(pthread_create(&thread, NULL, lcd, NULL) == 0);
    for (unsigned session = 0; session < 50; session++) {
        while (state() != LISP_APP_DISPLAY_TERMINAL)
            pause_tick();
        pthread_mutex_lock(&lock);
        assert(lisp_app_canvas_begin(&canvas));
        pthread_mutex_unlock(&lock);
        lisp_app_clear(draw(), (uint16_t)(session + 1));
        pthread_mutex_lock(&lock);
        assert(lisp_app_canvas_publish(&canvas));
        lisp_app_canvas_activate(&canvas);
        pthread_mutex_unlock(&lock);
        for (unsigned frame = 0; frame < 5; frame++) {
            while (state() != LISP_APP_DISPLAY_DRAWABLE)
                pause_tick();
            uint16_t *back = draw();
            assert(back);
            lisp_app_clear(back, (uint16_t)(session * 5 + frame + 2));
            pthread_mutex_lock(&lock);
            assert(lisp_app_canvas_publish(&canvas));
            pthread_mutex_unlock(&lock);
        }
        pthread_mutex_lock(&lock);
        lisp_app_canvas_restore(&canvas);
        pthread_mutex_unlock(&lock);
    }
    while (state() != LISP_APP_DISPLAY_TERMINAL)
        pause_tick();
    atomic_store(&halt, true);
    assert(pthread_join(thread, NULL) == 0);
    assert(atomic_load(&app_frames) >= 250);
    puts("PASS: host threads, immutable whole frames across 50 launch/restore cycles; no SPI/DMA claim");
    return 0;
}
