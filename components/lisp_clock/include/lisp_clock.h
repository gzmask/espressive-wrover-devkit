#pragma once
#include <stdbool.h>
#include <stdint.h>
#define LISP_CLOCK_EPOCH_MIN INT64_C(1735689600)
typedef struct {
    void *user;
    uint64_t (*now_ms)(void *);
    int64_t (*unix_seconds)(void *);
    bool (*cancelled)(void *);
    bool (*try_lock)(void *);
    void (*unlock)(void *);
    bool (*start)(void *);
    bool (*wait)(void *, unsigned);
    void (*stop)(void *);
    void (*sleep)(void *, unsigned);
} lisp_clock_io_t;
/* Serialized, request-triggered, at most 10s sync wait within caller deadline.
 * Does not authenticate time or guarantee SDK calls are preemptible. */
bool lisp_clock_ensure(const lisp_clock_io_t *io, uint64_t deadline_ms);
/* ESP adapter shared by source loader and HTTP data worker. */
bool lisp_clock_service_ensure(uint64_t deadline_ms, bool (*cancelled)(void *), void *user);
