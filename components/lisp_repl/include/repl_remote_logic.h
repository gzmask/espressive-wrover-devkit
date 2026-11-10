#pragma once
#include <stdbool.h>
#include <stddef.h>
enum { REPL_REMOTE_SOURCE_CAPACITY = 1024, REPL_REMOTE_REPLY_CAPACITY = 2048 };
typedef struct {
    char text[REPL_REMOTE_REPLY_CAPACITY];
    size_t used;
    bool truncated;
} repl_remote_reply_t;
bool repl_remote_source_valid(const char *source, size_t length);
void repl_remote_reply_init(repl_remote_reply_t *reply);
void repl_remote_reply_append(repl_remote_reply_t *reply, const char *text, size_t length);
void repl_remote_reply_finish(repl_remote_reply_t *reply);
