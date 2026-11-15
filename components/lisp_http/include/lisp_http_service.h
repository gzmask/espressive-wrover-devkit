#pragma once
#include "lisp_http.h"
/* One opt-in worker, fixed PSRAM buffer, no boot request or worker restart. */
bool lisp_http_service_start(lisp_http_backend_t *backend);
