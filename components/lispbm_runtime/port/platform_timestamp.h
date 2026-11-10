#pragma once
#include <stdint.h>
/* Updated only by the same owner before stepping the evaluator. */
extern uint32_t wrover_lisp_timestamp_us;
static inline uint32_t lbm_timestamp(void)
{
    return wrover_lisp_timestamp_us;
}
