#include "firmware_health_logic.h"

firmware_health_state_t firmware_health_classify(firmware_health_inputs_t inputs)
{
    if (!inputs.startup_complete)
        return FIRMWARE_STARTING;
    return inputs.startup_passed && inputs.heartbeat_recent && inputs.display_healthy &&
                   inputs.repl_healthy && inputs.wifi_online
               ? FIRMWARE_HEALTHY
               : FIRMWARE_UNHEALTHY;
}
