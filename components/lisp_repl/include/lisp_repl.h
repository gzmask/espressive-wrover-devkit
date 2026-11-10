#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
esp_err_t lisp_repl_start(void);
bool lisp_repl_is_healthy(void);
/* Called by the TCP task, never executes Lisp there. Fixed single request,
 * eight-second bounded wait; timeout quarantines the mailbox until reboot. */
esp_err_t lisp_repl_remote_eval(const char *source, size_t length, char *reply, size_t capacity);
