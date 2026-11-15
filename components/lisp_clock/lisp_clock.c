#include "lisp_clock.h"
#include <stddef.h>
static bool remaining(const lisp_clock_io_t *io, uint64_t deadline)
{
    return !io->cancelled(io->user) && io->now_ms(io->user) < deadline;
}
bool lisp_clock_ensure(const lisp_clock_io_t *io, uint64_t deadline)
{
    if (!io || !io->now_ms || !io->unix_seconds || !io->cancelled || !io->try_lock || !io->unlock ||
        !io->start || !io->wait || !io->stop || !io->sleep)
        return false;
    while (remaining(io, deadline)) {
        if (io->unix_seconds(io->user) >= LISP_CLOCK_EPOCH_MIN)
            return true;
        if (!io->try_lock(io->user)) {
            io->sleep(io->user, 10);
            continue;
        }
        bool synced = io->unix_seconds(io->user) >= LISP_CLOCK_EPOCH_MIN;
        if (!synced && remaining(io, deadline) && io->start(io->user)) {
            uint64_t until = io->now_ms(io->user) + 10000;
            if (until > deadline)
                until = deadline;
            while (remaining(io, until)) {
                uint64_t now = io->now_ms(io->user);
                if (now >= until)
                    break;
                uint64_t left = until - now;
                unsigned wait = left < 100 ? (unsigned)left : 100;
                if (!wait)
                    break;
                if (io->wait(io->user, wait)) {
                    synced = io->unix_seconds(io->user) >= LISP_CLOCK_EPOCH_MIN;
                    if (synced)
                        break;
                }
            }
            io->stop(io->user);
        }
        io->unlock(io->user);
        return synced && remaining(io, deadline);
    }
    return false;
}
