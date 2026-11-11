#include "display_pipeline.h"

#include <stdint.h>

static display_pipeline_result_t result(display_pipeline_status_t status, int error, bool uncertain)
{
    return (display_pipeline_result_t){status, error, uncertain};
}

display_pipeline_result_t display_pipeline_run(const display_pipeline_ops_t *ops, uint8_t *first,
                                               uint8_t *second, size_t buffer_bytes, unsigned height,
                                               unsigned rows)
{
    if (!ops || !ops->render || !ops->submit || !ops->wait || !first || !second || !buffer_bytes || !height ||
        !rows || height % rows != 0)
        return result(DISPLAY_PIPELINE_INVALID, 0, false);
    uintptr_t a = (uintptr_t)first, b = (uintptr_t)second;
    if (buffer_bytes > UINTPTR_MAX - a || buffer_bytes > UINTPTR_MAX - b ||
        !(a + buffer_bytes <= b || b + buffer_bytes <= a))
        return result(DISPLAY_PIPELINE_INVALID, 0, false);

    uint8_t *buffers[2] = {first, second};
    unsigned index = 0;
    bool pending = false;
    for (unsigned row = 0; row < height; row += rows) {
        uint8_t *buffer = buffers[index];
        int error = ops->render(ops->user, buffer, row, rows);
        if (error)
            return result(DISPLAY_PIPELINE_RENDER_ERROR, error, pending);
        if (pending) {
            error = ops->wait(ops->user);
            if (error)
                return result(DISPLAY_PIPELINE_WAIT_ERROR, error, true);
        }
        /* A submit error may have queued DMA before failing. */
        error = ops->submit(ops->user, buffer, row, rows);
        if (error)
            return result(DISPLAY_PIPELINE_SUBMIT_ERROR, error, true);
        pending = true;
        index ^= 1U;
    }
    int error = ops->wait(ops->user);
    if (error)
        return result(DISPLAY_PIPELINE_WAIT_ERROR, error, true);
    return result(DISPLAY_PIPELINE_OK, 0, false);
}
