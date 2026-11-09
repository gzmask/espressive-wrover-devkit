#pragma once
#include "wifi_link_logic.h"
#include "wifi_scan_logic.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { DISPLAY_WIDTH = 320, DISPLAY_HEIGHT = 240, DISPLAY_STRIP_ROWS = 16 };
typedef enum { DISPLAY_UNKNOWN, DISPLAY_ILI9341, DISPLAY_ST7789 } display_model_t;
typedef struct {
    uint8_t rdid[3];
    uint8_t id4[2];
} display_id_sample_t;
typedef struct {
    display_model_t model;
    bool id_match;
    uint64_t uptime_s;
    uint32_t free_heap_bytes;
    const char *firmware;
    wifi_scan_snapshot_t wifi;
    wifi_link_snapshot_t link;
} display_probe_view_t;

/* Exact known signatures in two consistent reads; never infer from zero/FF. */
display_model_t display_probe_identify(const display_id_sample_t *a, const display_id_sample_t *b);
/* Confirmed ILI9341 only; unknown/other IDs clear the plan and fail closed. */
size_t display_probe_plan(display_model_t hint, display_model_t out[2]);
const char *display_probe_model_name(display_model_t model);
/* Writes RGB565 in big-endian wire order, into only the supplied strip buffer. */
bool display_probe_render_strip(uint8_t *out, size_t capacity, unsigned first_row, unsigned rows,
                                const display_probe_view_t *view);
