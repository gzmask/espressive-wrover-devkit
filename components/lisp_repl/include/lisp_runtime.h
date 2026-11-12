#pragma once
#include "display_lifecycle.h"
#include "lisp_app.h"
#include "lisp_app_url.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*lisp_output_fn)(void *user, const char *text, size_t length);
/* Singleton, all calls on ONE owner thread. Caller storage lives until reboot. */
enum { LISP_RUNTIME_STORAGE_ALIGNMENT = 8 };
size_t lisp_runtime_storage_size(void);
size_t lisp_runtime_storage_alignment(void);
/* Last init/reset stage: storage, memory, symbols, heap, print, extensions,
 * evaluator, image, print-extension or ready. Only the owner reads this. */
const char *lisp_runtime_init_stage(void);
bool lisp_runtime_init(void *storage, size_t capacity, lisp_output_fn output, void *user);
/* Copies source; caller may reuse it immediately. Rejects busy/oversize input. */
bool lisp_runtime_submit(const char *source, size_t length, uint64_t now_ms);
void lisp_runtime_step(uint64_t now_ms);
bool lisp_runtime_busy(void);
bool lisp_runtime_ready(void);
/* Trusted boot probe; service health is withheld until actual (+ 1 2) == 3. */
bool lisp_runtime_selftest_start(uint64_t now_ms);
bool lisp_runtime_selftest_passed(void);
/* RAM-only reset; cancels ALL contexts and forgets definitions. No device reset. */
bool lisp_runtime_reset(void);
/* URL loader: asynchronous request, fixed RAM, no automatic app-run.
 * All APIs on original owner; attachment enables app-load-url. */
bool lisp_runtime_load_attach(const lisp_app_url_backend_t *backend);
bool lisp_runtime_load_active(void);
const char *lisp_runtime_load_status(void);
/* Download cancel preserves definitions; evaluation cancel resets all RAM. */
void lisp_runtime_load_cancel(void);
/* Owner control plane: exact :load-status / :load-cancel; no source evaluation. */
bool lisp_runtime_load_control(const char *source, size_t length, char *reply, size_t capacity);
/* Same owner only. App callbacks are separate from request busy/5s deadlines. */
bool lisp_runtime_lcd_attach(const display_lifecycle_backend_t *backend);
bool lisp_runtime_app_attach(const lisp_app_display_t *display);
bool lisp_runtime_app_active(void);
void lisp_runtime_app_stop(void);
bool lisp_runtime_app_key(uint8_t key);
/* Returns true when consumed as an app byte; UART Ctrl-C requests stop. */
bool lisp_runtime_app_input(uint8_t byte);
typedef enum { LISP_APP_COMMAND_NONE, LISP_APP_COMMAND_OK, LISP_APP_COMMAND_INVALID } lisp_app_command_t;
/* Control plane bypasses ordinary eval busy; never evaluates source. */
lisp_app_command_t lisp_runtime_app_control(const char *source, size_t length);
