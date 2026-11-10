#pragma once
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef TEST_LBM32_LOW_MEMORY
#include <sys/mman.h>
/* A 32-bit LispBM VALUE model on a 64-bit host. No MAP_FIXED: never replace an
 * existing mapping. This is not a claim of a native Xtensa/32-bit-pointer ABI. */
static inline void *lisp_test_allocate(size_t size)
{
    void *memory =
        mmap((void *)(uintptr_t)0x10000000, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(memory != MAP_FAILED && (uintptr_t)memory <= UINT32_MAX - size);
    return memory;
}
static inline void lisp_test_free(void *memory, size_t size)
{
    assert(munmap(memory, size) == 0);
}
#else
static inline void *lisp_test_allocate(size_t size)
{
    return calloc(1, size);
}
static inline void lisp_test_free(void *memory, size_t size)
{
    (void)size;
    free(memory);
}
#endif
