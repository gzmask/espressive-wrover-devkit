#include "firmware_health_logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static firmware_health_inputs_t good(void)
{
    return (firmware_health_inputs_t){.startup_complete = true,
                                      .startup_passed = true,
                                      .heartbeat_recent = true,
                                      .display_healthy = true,
                                      .repl_healthy = true,
                                      .wifi_online = true};
}
static void starting(void)
{
    firmware_health_inputs_t inputs = {0};
    assert(firmware_health_classify(inputs) == FIRMWARE_STARTING);
}
static void healthy(void)
{
    assert(firmware_health_classify(good()) == FIRMWARE_HEALTHY);
}
static void failures(void)
{
    firmware_health_inputs_t inputs = good();
    inputs.startup_passed = false;
    assert(firmware_health_classify(inputs) == FIRMWARE_UNHEALTHY);
    inputs = good();
    inputs.heartbeat_recent = false;
    assert(firmware_health_classify(inputs) == FIRMWARE_UNHEALTHY);
    inputs = good();
    inputs.display_healthy = false;
    assert(firmware_health_classify(inputs) == FIRMWARE_UNHEALTHY);
    inputs = good();
    inputs.repl_healthy = false;
    assert(firmware_health_classify(inputs) == FIRMWARE_UNHEALTHY);
    inputs = good();
    inputs.wifi_online = false;
    assert(firmware_health_classify(inputs) == FIRMWARE_UNHEALTHY);
    inputs.wifi_online = true;
    assert(firmware_health_classify(inputs) == FIRMWARE_HEALTHY);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "starting"))
        starting();
    else if (!strcmp(argv[1], "healthy"))
        healthy();
    else if (!strcmp(argv[1], "failures"))
        failures();
    else
        return 1;
    puts("PASS");
    return 0;
}
