#pragma once
#include "lisp_runtime.h"
#include "lispbm.h"
bool lisp_app_runtime_register(lisp_output_fn output, void *user);
void lisp_app_runtime_reset(void);
void lisp_app_runtime_launch_finished(bool success);
bool lisp_app_runtime_done(eval_context_t *context);
void lisp_app_runtime_step(uint64_t now_ms);
void lisp_runtime_limit_output(size_t bytes);
bool lisp_runtime_output_dropped(void);
