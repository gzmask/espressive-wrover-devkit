#pragma once
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
