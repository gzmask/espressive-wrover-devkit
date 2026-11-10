#pragma once
#include "lisp_app_url.h"
/* Start only when the opt-in feature is enabled. Idle worker does no network IO.
 * Startup/allocation failure is terminal until reboot; ordinary REPL survives. */
bool lisp_app_url_service_start(lisp_app_url_backend_t *backend);
