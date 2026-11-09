#include "node_status.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>

#include "cJSON.h"

node_status_t node_status_initial(void)
{
    return (node_status_t){.phase = NODE_STARTING};
}

bool node_status_receive(node_status_t *status, const node_sample_t *sample)
{
    if (status == NULL || sample == NULL || sample->uptime_ms > NODE_STATUS_MAX_UPTIME_MS) {
        return false;
    }
    if (status->heartbeat_count != 0 && sample->uptime_ms <= status->latest.uptime_ms) {
        return false;
    }
    status->latest = *sample;
    status->phase = NODE_RUNNING;
    if (status->heartbeat_count < UINT32_MAX) {
        status->heartbeat_count++;
    }
    return true;
}

void node_status_mark_stalled(node_status_t *status)
{
    if (status != NULL) {
        status->phase = NODE_STALLED;
    }
}

static const char *phase_name(node_phase_t phase)
{
    switch (phase) {
    case NODE_STARTING:
        return "starting";
    case NODE_RUNNING:
        return "running";
    case NODE_STALLED:
        return "stalled";
    default:
        return NULL;
    }
}

bool node_status_format_json(const node_status_t *status, const char *firmware, char *buffer, size_t capacity)
{
    if (buffer == NULL || capacity == 0) {
        return false;
    }
    buffer[0] = '\0';
    if (status == NULL || firmware == NULL || capacity > INT_MAX ||
        status->latest.uptime_ms > NODE_STATUS_MAX_UPTIME_MS) {
        return false;
    }
    const char *phase = phase_name(status->phase);
    if (phase == NULL) {
        return false;
    }
    /* cJSON's 15-digit double formatting may round very large integers. Only
     * internally generated decimal digits are passed as raw JSON here. */
    char uptime_decimal[32];
    int length = snprintf(uptime_decimal, sizeof(uptime_decimal), "%" PRIu64, status->latest.uptime_ms);
    if (length < 0 || (size_t)length >= sizeof(uptime_decimal)) {
        return false;
    }
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return false;
    }
    bool complete =
        cJSON_AddStringToObject(json, "firmware", firmware) != NULL &&
        cJSON_AddStringToObject(json, "state", phase) != NULL &&
        cJSON_AddRawToObject(json, "uptime_ms", uptime_decimal) != NULL &&
        cJSON_AddNumberToObject(json, "heartbeat_count", status->heartbeat_count) != NULL &&
        cJSON_AddNumberToObject(json, "free_heap_bytes", status->latest.free_heap_bytes) != NULL &&
        cJSON_AddNumberToObject(json, "min_free_heap_bytes", status->latest.min_free_heap_bytes) != NULL &&
        cJSON_AddNumberToObject(json, "free_internal_heap_bytes", status->latest.free_internal_heap_bytes) !=
            NULL;
    if (complete) {
        complete = status->phase == NODE_STALLED
                       ? cJSON_AddStringToObject(json, "last_error", "heartbeat_timeout") != NULL
                       : cJSON_AddNullToObject(json, "last_error") != NULL;
    }
    bool success = complete && cJSON_PrintPreallocated(json, buffer, (int)capacity, false);
    cJSON_Delete(json);
    if (!success) {
        buffer[0] = '\0';
    }
    return success;
}
