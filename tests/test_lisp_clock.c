#include "lisp_clock.h"
#include <assert.h>
#include <stdio.h>
typedef struct {
    uint64_t now;
    int64_t utc;
    unsigned starts, stops, waits, locks, unlocks, contention;
    bool cancel, start_ok, sync_ok, cancel_wait, peer_set;
} fake_t;
static uint64_t now_ms(void *u)
{
    return ((fake_t *)u)->now;
}
static int64_t utc(void *u)
{
    return ((fake_t *)u)->utc;
}
static bool cancel(void *u)
{
    return ((fake_t *)u)->cancel;
}
static bool lock(void *u)
{
    fake_t *f = u;
    f->locks++;
    if (f->contention) {
        f->contention--;
        return false;
    }
    return true;
}
static void unlock(void *u)
{
    ((fake_t *)u)->unlocks++;
}
static bool start(void *u)
{
    fake_t *f = u;
    f->starts++;
    return f->start_ok;
}
static bool wait_sync(void *u, unsigned n)
{
    fake_t *f = u;
    f->now += n;
    f->waits++;
    if (f->cancel_wait)
        f->cancel = true;
    if (f->sync_ok)
        f->utc = LISP_CLOCK_EPOCH_MIN;
    return f->sync_ok;
}
static void stop(void *u)
{
    ((fake_t *)u)->stops++;
}
static void sleep_ms(void *u, unsigned n)
{
    fake_t *f = u;
    f->now += n;
    if (f->peer_set)
        f->utc = LISP_CLOCK_EPOCH_MIN;
}
int main(void)
{
    fake_t f = {.start_ok = true, .sync_ok = true};
    lisp_clock_io_t io = {&f, now_ms, utc, cancel, lock, unlock, start, wait_sync, stop, sleep_ms};
    assert(lisp_clock_ensure(&io, 30000) && f.starts == 1 && f.stops == 1 && f.unlocks == 1);
    assert(lisp_clock_ensure(&io, 30000) && f.starts == 1);
    f = (fake_t){.start_ok = true};
    assert(!lisp_clock_ensure(&io, 30000) && f.now == 10000 && f.stops == 1 && f.unlocks == 1);
    f = (fake_t){.start_ok = true};
    assert(!lisp_clock_ensure(&io, 250) && f.now == 250 && f.stops == 1);
    f = (fake_t){0};
    assert(!lisp_clock_ensure(&io, 30000) && !f.stops && f.unlocks == 1);
    f = (fake_t){.cancel = true};
    assert(!lisp_clock_ensure(&io, 30000) && !f.starts && !f.locks);
    f = (fake_t){.start_ok = true, .sync_ok = true, .contention = 3};
    assert(lisp_clock_ensure(&io, 30000) && f.locks == 4 && f.unlocks == 1 && f.now == 130);
    f = (fake_t){.contention = 100};
    assert(!lisp_clock_ensure(&io, 30) && !f.starts && !f.unlocks);
    f = (fake_t){.start_ok = true, .cancel_wait = true};
    assert(!lisp_clock_ensure(&io, 30000) && f.stops == 1 && f.unlocks == 1 && f.waits == 1);
    f = (fake_t){.contention = 1, .peer_set = true};
    assert(lisp_clock_ensure(&io, 30000) && !f.starts && !f.unlocks);
    puts("PASS: shared clock initialization, contention, deadline and cleanup");
}
