#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Single producer and at most one submitted transfer. A successful wait must
 * consume that transfer's completion before its buffer can be rendered again. */
typedef struct {
    void *user;
    int (*render)(void *user, uint8_t *buffer, unsigned first_row, unsigned rows);
    int (*submit)(void *user, const uint8_t *buffer, unsigned first_row, unsigned rows);
    int (*wait)(void *user);
} display_pipeline_ops_t;

typedef enum {
    DISPLAY_PIPELINE_OK,
    DISPLAY_PIPELINE_INVALID,
    DISPLAY_PIPELINE_RENDER_ERROR,
    DISPLAY_PIPELINE_SUBMIT_ERROR,
    DISPLAY_PIPELINE_WAIT_ERROR,
} display_pipeline_status_t;

typedef struct {
    display_pipeline_status_t status;
    int error;
    bool dma_uncertain;
} display_pipeline_result_t;

/* buffer_bytes describes each complete, disjoint buffer. height must be an
 * exact multiple of rows. On uncertain DMA the caller must quarantine all
 * DMA resources and callback context; neither retry nor free them. */
display_pipeline_result_t display_pipeline_run(const display_pipeline_ops_t *ops, uint8_t *first,
                                               uint8_t *second, size_t buffer_bytes, unsigned height,
                                               unsigned rows);
