#include "lisp_runtime.h"
#include "lispbm.h"

/* Upstream lbm_image.h requires LispBM types to have been declared first. */
#include "lbm_image.h"
#include "repl_frontend.h"
#include <string.h>

enum {
    HEAP_CELLS = 8192,
    MEMORY_BLOCKS = 1024,
    IMAGE_WORDS = 8192,
    EXTENSIONS = 8,
    RESULT_CAPACITY = 512,
    STEP_BATCH = 64,
    EVAL_TIMEOUT_MS = 5000,
};
typedef struct {
    _Alignas(LISP_RUNTIME_STORAGE_ALIGNMENT) lbm_cons_t heap[HEAP_CELLS];
    lbm_uint memory[LBM_MEMORY_SIZE_BLOCKS_TO_WORDS(MEMORY_BLOCKS)];
    lbm_uint bitmap[LBM_MEMORY_BITMAP_SIZE(MEMORY_BLOCKS)];
    lbm_extension_t extensions[EXTENSIONS];
    uint32_t image[IMAGE_WORDS];
    char source[REPL_SOURCE_CAPACITY];
    lbm_string_channel_state_t reader_state;
    lbm_char_channel_t reader;
} runtime_storage_t;
_Static_assert(_Alignof(runtime_storage_t) == LISP_RUNTIME_STORAGE_ALIGNMENT,
               "LispBM requires an eight-byte aligned heap even on 32-bit targets");
static runtime_storage_t *storage;
static lisp_output_fn output;
static void *output_user;
static bool initialized, active;
static bool selftest_running, selftest_passed;
static const char *init_stage = "storage";
static lbm_cid root_id;
static uint64_t started_ms;
static size_t output_remaining;
static bool output_dropped;
uint32_t wrover_lisp_timestamp_us;

static void emit(const char *text)
{
    size_t length = strlen(text);
    if (length > output_remaining) {
        length = output_remaining;
        output_dropped = true;
    }
    if (output != NULL && length != 0)
        output(output_user, text, length);
    output_remaining -= length;
}
static bool ram_image_write(uint32_t data, int32_t index, bool constant_heap)
{
    (void)constant_heap;
    if (storage == NULL || index < 0 || index >= IMAGE_WORDS)
        return false;
    storage->image[index] = data;
    return true;
}
static void print_value(lbm_value value)
{
    char text[RESULT_CAPACITY];
    memset(text, 0, sizeof(text));
    if (lbm_print_value(text, sizeof(text), value))
        emit(text);
    else
        emit("<result too large>");
}
static lbm_value print_extension(lbm_value *arguments, lbm_uint count)
{
    for (lbm_uint i = 0; i < count; i++) {
        if (i != 0)
            emit(" ");
        print_value(arguments[i]);
    }
    emit("\n");
    return ENC_SYM_TRUE;
}
static void done(eval_context_t *context)
{
    if (context->id != root_id)
        return;
    output_remaining = RESULT_CAPACITY + 128;
    if (selftest_running) {
        selftest_passed = lbm_is_number(context->r) && lbm_dec_as_i32(context->r) == 3;
        selftest_running = false;
        if (!selftest_passed)
            emit("ERROR: LispBM self-test failed\n");
        active = false;
        return;
    }
    if (output_dropped)
        emit("WARNING: print output truncated\n");
    emit(lbm_is_error(context->r) ? "ERROR: " : "=> ");
    print_value(context->r);
    emit("\n");
    active = false;
}
size_t lisp_runtime_storage_size(void)
{
    return sizeof(runtime_storage_t);
}
size_t lisp_runtime_storage_alignment(void)
{
    return LISP_RUNTIME_STORAGE_ALIGNMENT;
}
const char *lisp_runtime_init_stage(void)
{
    return init_stage;
}
bool lisp_runtime_reset(void)
{
    init_stage = "storage";
    if (storage == NULL)
        return false;
    active = false;
    initialized = false;
    selftest_running = false;
    root_id = -1;
    memset(storage, 0, sizeof(*storage));
    /* Same order as pinned lbm_init(), but retain the exact failing stage. */
    init_stage = "memory";
    if (!lbm_memory_init(storage->memory, LBM_MEMORY_SIZE_BLOCKS_TO_WORDS(MEMORY_BLOCKS), storage->bitmap,
                         LBM_MEMORY_BITMAP_SIZE(MEMORY_BLOCKS)))
        return false;
    init_stage = "symbols";
    if (!lbm_symrepr_init())
        return false;
    init_stage = "heap";
    if (!lbm_heap_init(storage->heap, HEAP_CELLS, 256))
        return false;
    init_stage = "print";
    if (!lbm_print_init(256))
        return false;
    init_stage = "extensions";
    if (!lbm_extensions_init(storage->extensions, EXTENSIONS))
        return false;
    init_stage = "evaluator";
    if (!lbm_eval_init())
        return false;
    /* Upstream needs an image for constant symbols; this is bounded RAM, never Flash. */
    init_stage = "image";
    lbm_image_init(storage->image, IMAGE_WORDS, ram_image_write);
    lbm_image_create("wrover-repl");
    if (!lbm_image_exists() || !lbm_image_boot())
        return false;
    lbm_add_eval_symbols();
    lbm_set_ctx_done_callback(done);
    lbm_set_verbose(false);
    init_stage = "print-extension";
    if (!lbm_add_extension("print", print_extension))
        return false;
    init_stage = "ready";
    initialized = true;
    return true;
}
bool lisp_runtime_init(void *memory, size_t capacity, lisp_output_fn callback, void *user)
{
    init_stage = "storage";
    if (memory == NULL || capacity < sizeof(runtime_storage_t) ||
        (uintptr_t)memory % LISP_RUNTIME_STORAGE_ALIGNMENT != 0 || storage != NULL)
        return false;
    storage = memory;
    selftest_passed = false;
    output = callback;
    output_user = user;
    return lisp_runtime_reset();
}
bool lisp_runtime_submit(const char *source, size_t length, uint64_t now_ms)
{
    if (!initialized || active || source == NULL || length == 0 || length >= sizeof(storage->source) ||
        memchr(source, 0, length) != NULL)
        return false;
    /* Interop launcher allocation does not trigger GC on its own. Collect
     * before replacing the previous reader buffer, on this same owner. */
    (void)lbm_perform_gc();
    memcpy(storage->source, source, length);
    storage->source[length] = 0;
    wrover_lisp_timestamp_us = (uint32_t)(now_ms * 1000);
    lbm_create_string_char_channel(&storage->reader_state, &storage->reader, storage->source);
    output_remaining = 8192;
    root_id = lbm_load_and_eval_program(&storage->reader, NULL);
    if (root_id < 0) {
        emit("ERROR: cannot allocate evaluation context; use :reset\n");
        return false;
    }
    started_ms = now_ms;
    output_dropped = false;
    active = true;
    return true;
}
static void has_context(eval_context_t *context, void *found, void *unused)
{
    (void)context;
    (void)unused;
    *(bool *)found = true;
}
void lisp_runtime_step(uint64_t now_ms)
{
    if (!initialized || !active)
        return;
    if (now_ms - started_ms >= EVAL_TIMEOUT_MS) {
        output_remaining = RESULT_CAPACITY;
        emit("ERROR: evaluation timeout; RAM definitions cleared\n");
        if (!lisp_runtime_reset())
            emit("ERROR: runtime reset failed\n");
        return;
    }
    wrover_lisp_timestamp_us = (uint32_t)(now_ms * 1000);
    (void)lbm_eval_step(STEP_BATCH);
    if (!active) {
        bool remaining = false;
        lbm_all_ctxs_iterator(has_context, &remaining, NULL);
        if (remaining) {
            emit("ERROR: detached contexts unsupported; RAM definitions cleared\n");
            if (!lisp_runtime_reset())
                emit("ERROR: runtime reset failed\n");
        }
    }
}
bool lisp_runtime_busy(void)
{
    return active;
}
bool lisp_runtime_ready(void)
{
    return initialized;
}
bool lisp_runtime_selftest_start(uint64_t now_ms)
{
    if (!initialized || active)
        return false;
    selftest_passed = false;
    if (!lisp_runtime_submit("(+ 1 2)", 7, now_ms))
        return false;
    selftest_running = true;
    return true;
}
bool lisp_runtime_selftest_passed(void)
{
    return selftest_passed;
}
