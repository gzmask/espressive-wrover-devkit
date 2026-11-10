#pragma once
#include "lcd_terminal.h"

enum { REPL_SOURCE_CAPACITY = 1024 };
typedef enum { REPL_IGNORED, REPL_CHANGED, REPL_READY, REPL_CANCELLED, REPL_OVERFLOW } repl_input_event_t;
typedef struct {
    lcd_terminal_t terminal;
    char source[REPL_SOURCE_CAPACITY];
    size_t length, line_start;
    unsigned escape_state;
    bool suppress_lf, ready, overflowed;
} repl_frontend_t;
void repl_frontend_init(repl_frontend_t *frontend);
repl_input_event_t repl_frontend_feed(repl_frontend_t *frontend, uint8_t byte);
void repl_frontend_finish(repl_frontend_t *frontend);
/* Scanner is only a continuation hint; syntax validation belongs to LispBM. */
bool repl_source_needs_more(const char *source, size_t length);
