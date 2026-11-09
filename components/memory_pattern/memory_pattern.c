#include "memory_pattern.h"

static bool valid_range(const volatile uint32_t *words, size_t count)
{
    return words != NULL && count > 0 && count <= SIZE_MAX / sizeof(*words) && count <= UINT32_MAX;
}

static uint32_t pattern(size_t index, uint32_t seed)
{
    return ((uint32_t)index * UINT32_C(0x9e3779b9)) ^ seed;
}

bool memory_pattern_fill(volatile uint32_t *words, size_t count, uint32_t seed)
{
    if (!valid_range(words, count)) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        words[i] = pattern(i, seed);
    }
    return true;
}

memory_pattern_result_t memory_pattern_check(const volatile uint32_t *words, size_t count, uint32_t seed,
                                             size_t *bad_word)
{
    if (bad_word != NULL) {
        *bad_word = SIZE_MAX;
    }
    if (!valid_range(words, count)) {
        return MEMORY_PATTERN_INVALID;
    }
    for (size_t i = 0; i < count; i++) {
        if (words[i] != pattern(i, seed)) {
            if (bad_word != NULL) {
                *bad_word = i;
            }
            return MEMORY_PATTERN_MISMATCH;
        }
    }
    return MEMORY_PATTERN_OK;
}
