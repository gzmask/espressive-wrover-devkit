#include "memory_pattern.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                                     \
    do {                                                                                                     \
        if (!(condition)) {                                                                                  \
            fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #condition);                           \
            exit(EXIT_FAILURE);                                                                              \
        }                                                                                                    \
    } while (0)

static void test_known(void)
{
    uint32_t words[4];
    const uint32_t expected[] = {0, UINT32_C(0x9e3779b9), UINT32_C(0x3c6ef372), UINT32_C(0xdaa66d2b)};
    CHECK(memory_pattern_fill(words, 4, 0));
    for (size_t i = 0; i < 4; i++) {
        CHECK(words[i] == expected[i]);
    }
    size_t bad = 0;
    CHECK(memory_pattern_check(expected, 4, 0, &bad) == MEMORY_PATTERN_OK);
    CHECK(bad == SIZE_MAX);
    CHECK(memory_pattern_fill(words, 4, UINT32_MAX));
    for (size_t i = 0; i < 4; i++) {
        CHECK(words[i] == ~expected[i]);
    }
}

static void test_roundtrip(void)
{
    const size_t count = 128 * 1024 / sizeof(uint32_t);
    uint32_t *allocation = malloc((count + 2) * sizeof(*allocation));
    CHECK(allocation != NULL);
    uint32_t *words = allocation + 1;
    allocation[0] = UINT32_C(0x12345678);
    allocation[count + 1] = UINT32_C(0x87654321);
    const uint32_t seeds[] = {0,
                              UINT32_MAX,
                              UINT32_C(0xaaaaaaaa),
                              UINT32_C(0x55555555),
                              UINT32_C(0x01234567),
                              UINT32_C(0xfedcba98)};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); i++) {
        CHECK(memory_pattern_fill(words, count, seeds[i]));
        CHECK(memory_pattern_check(words, count, seeds[i], NULL) == MEMORY_PATTERN_OK);
    }
    CHECK(allocation[0] == UINT32_C(0x12345678));
    CHECK(allocation[count + 1] == UINT32_C(0x87654321));
    free(allocation);
}

static void test_corruption(void)
{
    uint32_t words[64];
    for (size_t position = 0; position < 64; position++) {
        CHECK(memory_pattern_fill(words, 64, 42));
        words[position] ^= UINT32_C(0x80000001);
        size_t bad = SIZE_MAX;
        CHECK(memory_pattern_check(words, 64, 42, &bad) == MEMORY_PATTERN_MISMATCH);
        CHECK(bad == position);
    }
    CHECK(memory_pattern_fill(words, 64, 42));
    words[2] ^= 1;
    words[4] ^= 1;
    size_t bad = SIZE_MAX;
    CHECK(memory_pattern_check(words, 64, 42, &bad) == MEMORY_PATTERN_MISMATCH);
    CHECK(bad == 2);
    CHECK(memory_pattern_fill(words, 64, 42));
    CHECK(memory_pattern_check(words, 64, 43, NULL) == MEMORY_PATTERN_MISMATCH);
}

static void test_invalid(void)
{
    uint32_t word = 17;
    size_t bad = 0;
    CHECK(!memory_pattern_fill(NULL, 1, 0));
    CHECK(!memory_pattern_fill(&word, 0, 0));
    CHECK(!memory_pattern_fill(&word, SIZE_MAX, 0));
    CHECK(memory_pattern_check(NULL, 1, 0, &bad) == MEMORY_PATTERN_INVALID);
    CHECK(bad == SIZE_MAX);
    CHECK(memory_pattern_check(&word, 0, 0, NULL) == MEMORY_PATTERN_INVALID);
    CHECK(memory_pattern_check(&word, SIZE_MAX, 0, NULL) == MEMORY_PATTERN_INVALID);
    CHECK(word == 17);
}

int main(int argc, char **argv)
{
    const struct {
        const char *name;
        void (*run)(void);
    } tests[] = {
        {"known", test_known},
        {"roundtrip", test_roundtrip},
        {"corruption", test_corruption},
        {"invalid", test_invalid},
    };
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        if (strcmp(argv[1], tests[i].name) == 0) {
            tests[i].run();
            printf("PASS %s\n", tests[i].name);
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
}
