#pragma once
#include "lisp_http.h"
#include "lisp_json.h"
#include "lispbm.h"
typedef struct {
    lisp_json_document_t json;
    lbm_value values[LISP_JSON_NODES];
    char text[LISP_JSON_STRING_MAX + 1];
} lisp_data_workspace_t;
bool lisp_data_runtime_register(lisp_data_workspace_t *workspace);
void lisp_data_runtime_reset(void);
void lisp_data_runtime_step(uint64_t now_ms);
void lisp_data_runtime_app_stop(void);
bool lisp_runtime_http_attach(const lisp_http_backend_t *backend);
bool lisp_runtime_time_attach(int64_t (*unix_seconds)(void *), void *user);
