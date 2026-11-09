#pragma once
#include <stdbool.h>

/* One-shot check after heartbeat/status tasks start. Uses only its own PSRAM
 * allocation, then frees it; never retests the raw heap or touches Flash. */
bool psram_probe_run(void);
