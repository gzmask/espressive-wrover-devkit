#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef void (*repl_help_output_t)(void *user, const char *text, size_t length);

/* Render the same feature-aware help for UART, LCD and TCP. */
void repl_help_write(repl_help_output_t output, void *user, bool app, bool url, bool tcp);
/* Dedicated LCD lifecycle page; bounded to the same terminal/reply layout. */
void repl_help_lcd_write(repl_help_output_t output, void *user);
