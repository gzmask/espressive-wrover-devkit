#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum {
    LISP_JSON_NODES = 256,
    LISP_JSON_DEPTH = 12,
    LISP_JSON_STRING_MAX = 1024,
    LISP_JSON_BYTES = 8192,
    LISP_JSON_NONE = UINT16_MAX
};
typedef enum {
    LISP_JSON_OBJECT,
    LISP_JSON_ARRAY,
    LISP_JSON_STRING,
    LISP_JSON_INTEGER,
    LISP_JSON_NUMBER,
    LISP_JSON_TRUE,
    LISP_JSON_FALSE,
    LISP_JSON_NULL
} lisp_json_type_t;
typedef struct {
    uint16_t start, length, child, next;
    lisp_json_type_t type;
} lisp_json_token_t;
typedef struct {
    lisp_json_token_t tokens[LISP_JSON_NODES];
    uint16_t count;
} lisp_json_document_t;
/* Returned error name is static; NULL means success. No allocation or evaluation. */
const char *lisp_json_parse(lisp_json_document_t *doc, const char *text, size_t length);
bool lisp_json_string(const char *text, const lisp_json_token_t *token, char *out, size_t capacity,
                      size_t *length);
