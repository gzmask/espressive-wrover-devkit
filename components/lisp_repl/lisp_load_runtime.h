#pragma once
#include "lisp_runtime.h"
bool lisp_load_runtime_register(void);
void lisp_load_runtime_reset(void);
void lisp_load_runtime_failed(void);
void lisp_load_runtime_finished(bool success);
void lisp_load_runtime_step(uint64_t now_ms);
/* Internal owner-only path. Copies into separate, fixed PSRAM runtime storage. */
bool lisp_runtime_submit_loaded(const char *source, size_t length, uint64_t now_ms);
