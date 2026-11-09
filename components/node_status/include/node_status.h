#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Largest integer represented exactly by JSON consumers using IEEE-754 doubles. */
#define NODE_STATUS_MAX_UPTIME_MS UINT64_C(9007199254740991)

typedef enum {
    NODE_STARTING,
    NODE_RUNNING,
    NODE_STALLED,
} node_phase_t;

typedef struct {
    uint64_t uptime_ms;
    uint32_t free_heap_bytes;
    uint32_t min_free_heap_bytes;
    uint32_t free_internal_heap_bytes;
} node_sample_t;

/* Owned by the status task; other tasks send value copies through a queue. */
typedef struct {
    node_phase_t phase;
    uint32_t heartbeat_count;
    node_sample_t latest;
} node_status_t;

node_status_t node_status_initial(void);

/* Rejects duplicate/out-of-order samples without mutating state. */
bool node_status_receive(node_status_t *status, const node_sample_t *sample);
void node_status_mark_stalled(node_status_t *status);

/* No secrets. Caller owns the buffer; any failure leaves it empty when possible.
 * cJSON requires some spare space, so callers should use at least 512 bytes.
 * Allocation failure is reported, never fatal.
 */
bool node_status_format_json(const node_status_t *status, const char *firmware, char *buffer,
                             size_t capacity);
