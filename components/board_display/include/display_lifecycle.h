#pragma once
#include <stdbool.h>

typedef enum {
    DISPLAY_STARTING,
    DISPLAY_ON,
    DISPLAY_STOPPING,
    DISPLAY_OFF,
    DISPLAY_SD_BLOCKED,
    DISPLAY_FAULT
} display_lifecycle_state_t;

typedef struct {
    display_lifecycle_state_t state;
    bool sd_reserved;
} display_lifecycle_t;

/* Caller serializes all transitions. Fault is terminal until reboot. */
void display_lifecycle_init(display_lifecycle_t *display);
bool display_lifecycle_request(display_lifecycle_t *display, bool on, bool app_idle);
void display_lifecycle_complete(display_lifecycle_t *display, display_lifecycle_state_t state);
bool display_lifecycle_health(const display_lifecycle_t *display, bool frame_recent);
bool display_lifecycle_sd_acquire(display_lifecycle_t *display);
bool display_lifecycle_sd_release(display_lifecycle_t *display);

typedef enum {
    DISPLAY_RELEASE_BACKLIGHT,
    DISPLAY_RELEASE_PANEL_OFF,
    DISPLAY_RELEASE_PANEL_DELETE,
    DISPLAY_RELEASE_IO_DELETE,
    DISPLAY_RELEASE_BUS_FREE,
    DISPLAY_RELEASE_PINS,
    DISPLAY_RELEASE_STEPS
} display_release_step_t;
typedef struct {
    void *user;
    int (*step)(void *user, display_release_step_t step);
} display_release_ops_t;
/* First error stops the sequence. Never touch resources with uncertain DMA. */
int display_lifecycle_shutdown(const display_release_ops_t *ops, bool dma_uncertain);

typedef struct {
    void *user;
    bool (*request)(void *user, bool on);
    display_lifecycle_state_t (*status)(void *user);
} display_lifecycle_backend_t;
