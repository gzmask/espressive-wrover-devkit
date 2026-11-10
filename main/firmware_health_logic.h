#pragma once
#include <stdbool.h>

typedef enum { FIRMWARE_STARTING, FIRMWARE_HEALTHY, FIRMWARE_UNHEALTHY } firmware_health_state_t;
typedef struct {
    bool startup_complete, startup_passed, heartbeat_recent;
    bool display_healthy, repl_healthy, wifi_online;
} firmware_health_inputs_t;
/* Disabled features are supplied as healthy. No restart or Flash side effects. */
firmware_health_state_t firmware_health_classify(firmware_health_inputs_t inputs);
