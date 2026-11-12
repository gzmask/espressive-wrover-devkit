#include "display_lifecycle.h"
#include <stddef.h>

void display_lifecycle_init(display_lifecycle_t *display)
{
    *display = (display_lifecycle_t){DISPLAY_STARTING, false};
}
bool display_lifecycle_request(display_lifecycle_t *display, bool on, bool app_idle)
{
    if (!display || !app_idle || display->state == DISPLAY_FAULT)
        return false;
    if (on) {
        if (display->sd_reserved)
            return false;
        if (display->state == DISPLAY_ON)
            return true;
        if (display->state != DISPLAY_OFF && display->state != DISPLAY_SD_BLOCKED)
            return false;
        display->state = DISPLAY_STARTING;
        return true;
    }
    if (display->state == DISPLAY_OFF)
        return true;
    if (display->state == DISPLAY_SD_BLOCKED && !display->sd_reserved) {
        display->state = DISPLAY_OFF;
        return true;
    }
    if (display->state != DISPLAY_ON)
        return false;
    display->state = DISPLAY_STOPPING;
    return true;
}
void display_lifecycle_complete(display_lifecycle_t *display, display_lifecycle_state_t state)
{
    if (!display || display->state == DISPLAY_FAULT)
        return;
    if (state == DISPLAY_FAULT ||
        (display->state == DISPLAY_STARTING && (state == DISPLAY_ON || state == DISPLAY_SD_BLOCKED)) ||
        (display->state == DISPLAY_STOPPING && state == DISPLAY_OFF))
        display->state = state;
}
bool display_lifecycle_health(const display_lifecycle_t *display, bool frame_recent)
{
    return display && ((display->state == DISPLAY_ON && frame_recent) || display->state == DISPLAY_OFF ||
                       display->state == DISPLAY_SD_BLOCKED);
}
bool display_lifecycle_sd_acquire(display_lifecycle_t *display)
{
    if (!display || display->sd_reserved ||
        (display->state != DISPLAY_OFF && display->state != DISPLAY_SD_BLOCKED))
        return false;
    display->sd_reserved = true;
    return true;
}
bool display_lifecycle_sd_release(display_lifecycle_t *display)
{
    if (!display || !display->sd_reserved)
        return false;
    display->sd_reserved = false;
    return true;
}
int display_lifecycle_shutdown(const display_release_ops_t *ops, bool dma_uncertain)
{
    if (!ops || !ops->step || dma_uncertain)
        return -1;
    for (unsigned i = 0; i < DISPLAY_RELEASE_STEPS; i++) {
        int error = ops->step(ops->user, (display_release_step_t)i);
        if (error != 0)
            return error;
    }
    return 0;
}
