#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef void (*repl_help_output_t)(void *user, const char *text, size_t length);

/* Render the feature-aware help for UART, LCD and TCP REPLs. */
void repl_help_write(repl_help_output_t output, void *user, bool app, bool url, bool tcp, bool sd);
/* Dedicated LCD and SD lifecycle pages; bounded to the REPL/LCD layout. */
void repl_help_lcd_write(repl_help_output_t output, void *user, bool sd);
void repl_help_sd_write(repl_help_output_t output, void *user);
