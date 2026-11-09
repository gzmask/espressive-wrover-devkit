#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MEMORY_PATTERN_OK,
    MEMORY_PATTERN_INVALID,
    MEMORY_PATTERN_MISMATCH,
} memory_pattern_result_t;

/* Caller owns an aligned, exclusively held buffer of count uint32_t words.
 * Only that buffer is touched. Empty/overflowing ranges are invalid.
 * Volatile accesses keep the actual reads/writes observable to the compiler.
 */
bool memory_pattern_fill(volatile uint32_t *words, size_t count, uint32_t seed);

/* bad_word is optional; set to SIZE_MAX unless an actual mismatch is found. */
memory_pattern_result_t memory_pattern_check(const volatile uint32_t *words, size_t count, uint32_t seed,
                                             size_t *bad_word);
