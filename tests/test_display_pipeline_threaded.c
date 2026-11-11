#include "display_pipeline.h"

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { BUFFER_BYTES = 10240, STRIPS = 15, FRAMES = 50 };
static uint8_t buffers[2][BUFFER_BYTES];
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    const uint8_t *pending;
    unsigned generation, completed, frame, row;
    bool stop, corrupted;
} fixture_t;

static void *dma_worker(void *user)
{
    fixture_t *f = user;
    unsigned seen = 0;
    assert(pthread_mutex_lock(&f->lock) == 0);
    for (;;) {
        while (!f->stop && f->generation == seen)
            assert(pthread_cond_wait(&f->changed, &f->lock) == 0);
        if (f->stop)
            break;
        const uint8_t *buffer = f->pending;
        unsigned generation = f->generation;
        uint8_t expected = (uint8_t)(f->frame + f->row);
        assert(buffer);
        assert(pthread_mutex_unlock(&f->lock) == 0);
        bool corrupt = false;
        for (size_t i = 0; i < BUFFER_BYTES; i++) {
            if (buffer[i] != expected)
                corrupt = true;
            if ((i % 512U) == 0)
                sched_yield();
        }
        assert(pthread_mutex_lock(&f->lock) == 0);
        f->corrupted |= corrupt;
        f->completed = seen = generation;
        assert(pthread_cond_broadcast(&f->changed) == 0);
    }
    assert(pthread_mutex_unlock(&f->lock) == 0);
    return NULL;
}
static int render(void *user, uint8_t *buffer, unsigned row, unsigned rows)
{
    fixture_t *f = user;
    assert(rows == 1);
    assert(pthread_mutex_lock(&f->lock) == 0);
    assert(f->pending != buffer);
    unsigned frame = f->frame;
    assert(pthread_mutex_unlock(&f->lock) == 0);
    memset(buffer, (uint8_t)(frame + row), BUFFER_BYTES);
    sched_yield();
    return 0;
}
static int submit(void *user, const uint8_t *buffer, unsigned row, unsigned rows)
{
    fixture_t *f = user;
    assert(rows == 1);
    assert(pthread_mutex_lock(&f->lock) == 0);
    assert(!f->pending);
    f->pending = buffer;
    f->row = row;
    f->generation++;
    assert(pthread_cond_broadcast(&f->changed) == 0);
    assert(pthread_mutex_unlock(&f->lock) == 0);
    return 0;
}
static int wait_done(void *user)
{
    fixture_t *f = user;
    assert(pthread_mutex_lock(&f->lock) == 0);
    while (f->completed != f->generation)
        assert(pthread_cond_wait(&f->changed, &f->lock) == 0);
    assert(f->pending && !f->corrupted);
    f->pending = NULL;
    assert(pthread_mutex_unlock(&f->lock) == 0);
    return 0;
}
int main(void)
{
    fixture_t f = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    pthread_t worker;
    assert(pthread_create(&worker, NULL, dma_worker, &f) == 0);
    const display_pipeline_ops_t ops = {&f, render, submit, wait_done};
    for (unsigned frame = 0; frame < FRAMES; frame++) {
        assert(pthread_mutex_lock(&f.lock) == 0);
        f.frame = frame;
        assert(pthread_mutex_unlock(&f.lock) == 0);
        display_pipeline_result_t result =
            display_pipeline_run(&ops, buffers[0], buffers[1], BUFFER_BYTES, STRIPS, 1);
        assert(result.status == DISPLAY_PIPELINE_OK && !result.dma_uncertain);
    }
    assert(pthread_mutex_lock(&f.lock) == 0);
    assert(!f.pending && !f.corrupted && f.completed == FRAMES * STRIPS);
    f.stop = true;
    assert(pthread_cond_broadcast(&f.changed) == 0);
    assert(pthread_mutex_unlock(&f.lock) == 0);
    assert(pthread_join(worker, NULL) == 0);
    assert(pthread_mutex_destroy(&f.lock) == 0);
    assert(pthread_cond_destroy(&f.changed) == 0);
    puts("PASS: 50 frames/750 asynchronous transfers retain buffer ownership");
    return 0;
}
