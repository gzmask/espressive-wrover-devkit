#include "node_status.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

#define CHECK(condition)                                                                                     \
    do {                                                                                                     \
        if (!(condition)) {                                                                                  \
            fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #condition);                           \
            exit(EXIT_FAILURE);                                                                              \
        }                                                                                                    \
    } while (0)

static const node_sample_t sample = {
    .uptime_ms = 1000,
    .free_heap_bytes = 200000,
    .min_free_heap_bytes = 199000,
    .free_internal_heap_bytes = 198000,
};

static cJSON *parse_status(const node_status_t *status, const char *firmware)
{
    char buffer[512];
    CHECK(node_status_format_json(status, firmware, buffer, sizeof(buffer)));
    cJSON *json = cJSON_ParseWithOpts(buffer, NULL, true);
    CHECK(json != NULL);
    return json;
}

static void check_string(const cJSON *json, const char *name, const char *expected)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(json, name);
    CHECK(cJSON_IsString(item));
    CHECK(strcmp(item->valuestring, expected) == 0);
}

static void check_number(const cJSON *json, const char *name, double expected)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(json, name);
    CHECK(cJSON_IsNumber(item));
    CHECK(item->valuedouble == expected);
}

static void test_initial(void)
{
    const node_status_t status = node_status_initial();
    CHECK(status.phase == NODE_STARTING);
    CHECK(status.heartbeat_count == 0);
    cJSON *json = parse_status(&status, "0.1.0");
    check_string(json, "firmware", "0.1.0");
    check_string(json, "state", "starting");
    check_number(json, "uptime_ms", 0);
    CHECK(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(json, "last_error")));
    CHECK(cJSON_GetObjectItemCaseSensitive(json, "password") == NULL);
    cJSON_Delete(json);
}

static void test_transitions(void)
{
    node_status_t status = node_status_initial();
    CHECK(node_status_receive(&status, &sample));
    CHECK(status.phase == NODE_RUNNING);
    CHECK(status.heartbeat_count == 1);
    cJSON *json = parse_status(&status, "0.1.0");
    check_string(json, "state", "running");
    check_number(json, "uptime_ms", 1000);
    check_number(json, "free_heap_bytes", 200000);
    check_number(json, "min_free_heap_bytes", 199000);
    check_number(json, "free_internal_heap_bytes", 198000);
    cJSON_Delete(json);
    node_status_mark_stalled(&status);
    node_status_mark_stalled(&status);
    CHECK(status.phase == NODE_STALLED);
    json = parse_status(&status, "0.1.0");
    check_string(json, "last_error", "heartbeat_timeout");
    cJSON_Delete(json);
    node_sample_t next = sample;
    next.uptime_ms++;
    CHECK(node_status_receive(&status, &next));
    CHECK(status.phase == NODE_RUNNING);
    CHECK(status.heartbeat_count == 2);
    json = parse_status(&status, "0.1.0");
    CHECK(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(json, "last_error")));
    cJSON_Delete(json);
}

static void test_ordering(void)
{
    node_status_t status = node_status_initial();
    node_sample_t next = sample;
    next.uptime_ms = 0;
    CHECK(node_status_receive(&status, &next));
    CHECK(!node_status_receive(&status, &next));
    CHECK(node_status_receive(&status, &sample));
    next.uptime_ms = sample.uptime_ms - 1;
    next.free_heap_bytes = 1;
    CHECK(!node_status_receive(&status, &next));
    CHECK(status.heartbeat_count == 2);
    CHECK(status.latest.uptime_ms == sample.uptime_ms);
    CHECK(status.latest.free_heap_bytes == sample.free_heap_bytes);
    node_status_mark_stalled(&status);
    CHECK(!node_status_receive(&status, &sample));
    CHECK(status.phase == NODE_STALLED);
}

static void test_escaping(void)
{
    const node_status_t status = node_status_initial();
    const char *firmware = "release \"test\" \\ \n\r\t\001 中文";
    cJSON *json = parse_status(&status, firmware);
    check_string(json, "firmware", firmware);
    CHECK(cJSON_GetArraySize(json) == 8);
    cJSON_Delete(json);
}

static void test_bounds(void)
{
    const node_status_t status = node_status_initial();
    char storage[514];
    for (size_t capacity = 0; capacity <= 512; capacity++) {
        memset(storage, '#', sizeof(storage));
        bool ok = node_status_format_json(&status, "0.1.0", &storage[1], capacity);
        CHECK(storage[0] == '#');
        CHECK(storage[capacity + 1] == '#');
        if (ok) {
            CHECK(capacity > 0);
            CHECK(memchr(&storage[1], '\0', capacity) != NULL);
            cJSON *json = cJSON_ParseWithOpts(&storage[1], NULL, true);
            CHECK(json != NULL);
            cJSON_Delete(json);
        } else if (capacity > 0) {
            CHECK(storage[1] == '\0');
        }
    }
    char tiny[1] = {'x'};
    CHECK(!node_status_format_json(&status, "0.1.0", tiny, sizeof(tiny)));
    CHECK(tiny[0] == '\0');
}

static void test_invalid(void)
{
    node_status_t status = node_status_initial();
    char buffer[512] = "old data";
    CHECK(!node_status_receive(NULL, &sample));
    CHECK(!node_status_receive(&status, NULL));
    node_status_mark_stalled(NULL);
    CHECK(!node_status_format_json(NULL, "0.1.0", buffer, sizeof(buffer)));
    CHECK(buffer[0] == '\0');
    CHECK(!node_status_format_json(&status, NULL, buffer, sizeof(buffer)));
    CHECK(!node_status_format_json(&status, "0.1.0", NULL, 512));
    CHECK(!node_status_format_json(&status, "0.1.0", buffer, (size_t)INT_MAX + 1));
    status.phase = (node_phase_t)99;
    CHECK(!node_status_format_json(&status, "0.1.0", buffer, sizeof(buffer)));
}

static void test_extremes(void)
{
    node_status_t status = node_status_initial();
    node_sample_t next = sample;
    next.uptime_ms = NODE_STATUS_MAX_UPTIME_MS + 1;
    CHECK(!node_status_receive(&status, &next));
    CHECK(status.phase == NODE_STARTING);
    next.uptime_ms = NODE_STATUS_MAX_UPTIME_MS;
    next.free_heap_bytes = UINT32_MAX;
    CHECK(node_status_receive(&status, &next));
    cJSON *json = parse_status(&status, "0.1.0");
    check_number(json, "uptime_ms", (double)NODE_STATUS_MAX_UPTIME_MS);
    check_number(json, "free_heap_bytes", (double)UINT32_MAX);
    cJSON_Delete(json);
    status.latest.uptime_ms = NODE_STATUS_MAX_UPTIME_MS + 1;
    char buffer[512];
    CHECK(!node_status_format_json(&status, "0.1.0", buffer, sizeof(buffer)));
    status = node_status_initial();
    status.heartbeat_count = UINT32_MAX;
    CHECK(node_status_receive(&status, &sample));
    CHECK(status.heartbeat_count == UINT32_MAX);
}

static size_t allocation_budget;
static size_t outstanding_allocations;

static void *limited_malloc(size_t size)
{
    if (allocation_budget == 0) {
        return NULL;
    }
    allocation_budget--;
    void *pointer = malloc(size);
    if (pointer != NULL) {
        outstanding_allocations++;
    }
    return pointer;
}

static void limited_free(void *pointer)
{
    if (pointer != NULL) {
        CHECK(outstanding_allocations > 0);
        outstanding_allocations--;
        free(pointer);
    }
}

static void test_allocation(void)
{
    const node_status_t status = node_status_initial();
    cJSON_Hooks hooks = {.malloc_fn = limited_malloc, .free_fn = limited_free};
    bool succeeded = false;
    for (size_t budget = 0; budget < 64; budget++) {
        char buffer[512] = "stale content";
        allocation_budget = budget;
        outstanding_allocations = 0;
        cJSON_InitHooks(&hooks);
        bool ok = node_status_format_json(&status, "0.1.0", buffer, sizeof(buffer));
        CHECK(outstanding_allocations == 0);
        cJSON_InitHooks(NULL);
        if (ok) {
            succeeded = true;
            break;
        }
        CHECK(buffer[0] == '\0');
    }
    CHECK(succeeded);
}

int main(int argc, char **argv)
{
    const struct {
        const char *name;
        void (*run)(void);
    } tests[] = {
        {"initial", test_initial},   {"transitions", test_transitions}, {"ordering", test_ordering},
        {"escaping", test_escaping}, {"bounds", test_bounds},           {"invalid", test_invalid},
        {"extremes", test_extremes}, {"allocation", test_allocation},
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
