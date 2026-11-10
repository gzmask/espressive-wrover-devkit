#pragma once
#include <stdbool.h>
#ifndef LBM_SINGLE_THREADED
#error "No-op mutex port requires the single-owner LispBM evaluator"
#endif
/* Even LispBM's single-thread evaluator references uninitialized queue mutexes.
 * This port is deliberately allocation-free. No other task may call LispBM. */
typedef unsigned lbm_mutex_t;
static inline bool lbm_mutex_init(lbm_mutex_t *mutex)
{
    *mutex = 0;
    return true;
}
static inline void lbm_mutex_lock(lbm_mutex_t *mutex)
{
    (void)mutex;
}
static inline void lbm_mutex_unlock(lbm_mutex_t *mutex)
{
    (void)mutex;
}
