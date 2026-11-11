#include "lisp_app_runtime.h"
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#include <limits.h>
#include <string.h>
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
#include "lisp_perf.h"
static lisp_perf_token_t callback_timing, backpressure_timing;
#endif

enum {
    APP_STEP_BATCH = 64,
    APP_CALLBACK_TIMEOUT_MS = 5000,
    APP_RESTORE_TIMEOUT_MS = 3000,
    APP_FRAME_MS = 100,
    APP_KEYS = 16,
    APP_LOG_BYTES = 512
};
typedef enum { APP_IDLE, APP_QUEUED, APP_STARTING, APP_RUNNING, APP_RESTORING } app_state_t;
typedef enum { APP_INIT, APP_STEP, APP_EXIT } app_job_t;
static app_state_t state;
static app_job_t job;
static lisp_app_display_t display;
static lisp_output_fn output;
static void *output_user;
static bool attached, callback_active, callback_done, callback_error, callback_exit;
static bool stop_requested, init_done, exit_started, failed, dirty, restore_clock_started;
static lbm_cid callback_id = -1;
static uint16_t *draw_pixels;
static uint64_t clock_ms, started_ms, next_tick_ms, restore_started_ms;
static uint8_t keys[APP_KEYS];
static unsigned key_head, key_count;
static char root_name[] = "__wrover_app";
extern uint32_t wrover_lisp_timestamp_us;

static void log_message(const char *message)
{
    if (output)
        output(output_user, message, strlen(message));
}
bool lisp_runtime_app_active(void)
{
    return state != APP_IDLE;
}
bool lisp_runtime_app_attach(const lisp_app_display_t *backend)
{
    if (lisp_runtime_app_active() || !backend || !backend->begin || !backend->draw || !backend->publish ||
        !backend->activate || !backend->restore || !backend->state)
        return false;
    display = *backend;
    attached = true;
    return true;
}
void lisp_runtime_app_stop(void)
{
    if (lisp_runtime_app_active())
        stop_requested = true;
}
bool lisp_runtime_app_key(uint8_t key)
{
    if (!lisp_runtime_app_active() || state == APP_RESTORING || stop_requested || key == 0 || key > 127)
        return false;
    if (key_count == APP_KEYS) {
        failed = true;
        stop_requested = true;
        log_message("ERROR: application input queue full; stopping app\n");
        return false;
    }
    keys[(key_head + key_count) % APP_KEYS] = key;
    key_count++;
    return true;
}
bool lisp_runtime_app_input(uint8_t byte)
{
    if (!lisp_runtime_app_active())
        return false;
    if (byte == 3)
        lisp_runtime_app_stop();
    else
        (void)lisp_runtime_app_key(byte);
    return true;
}
lisp_app_command_t lisp_runtime_app_control(const char *source, size_t length)
{
    uint8_t key = 0;
    switch (lisp_app_parse_control(source, length, &key)) {
    case LISP_APP_CONTROL_STOP:
        lisp_runtime_app_stop();
        return LISP_APP_COMMAND_OK;
    case LISP_APP_CONTROL_KEY:
        return lisp_runtime_app_key(key) ? LISP_APP_COMMAND_OK : LISP_APP_COMMAND_INVALID;
    case LISP_APP_CONTROL_INVALID:
        return LISP_APP_COMMAND_INVALID;
    default:
        return LISP_APP_COMMAND_NONE;
    }
}
static void begin_restore(void)
{
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    callback_timing.active = false;
    backpressure_timing.active = false;
#endif
    draw_pixels = NULL;
    if (attached)
        display.restore(display.user);
    (void)lbm_undefine(root_name);
    state = APP_RESTORING;
    restore_clock_started = false;
    key_count = key_head = 0;
}
void lisp_app_runtime_reset(void)
{
    if (lisp_runtime_app_active()) {
        failed = true;
        begin_restore();
    }
    callback_active = callback_done = false;
    callback_id = -1;
    draw_pixels = NULL;
}
static bool closure_arity(lbm_value function, unsigned wanted)
{
    if (!lbm_is_closure(function))
        return false;
    lbm_value tail = lbm_cdr(function);
    if (!lbm_is_cons(tail))
        return false;
    lbm_value params = lbm_car(tail);
    for (unsigned i = 0; i < wanted; i++) {
        if (!lbm_is_cons(params) || !lbm_is_symbol(lbm_car(params)))
            return false;
        params = lbm_cdr(params);
    }
    if (params != ENC_SYM_NIL)
        return false;
    /* A closure has exactly parameters, body and environment after its tag.
     * Bounded checks, not list_length: user lists may be cyclic. */
    tail = lbm_cdr(tail);
    if (!lbm_is_cons(tail))
        return false;
    tail = lbm_cdr(tail);
    return lbm_is_cons(tail) && lbm_cdr(tail) == ENC_SYM_NIL;
}
static bool descriptor(lbm_value value, lbm_value functions[3])
{
    for (unsigned i = 0; i < 3; i++) {
        if (!lbm_is_cons(value))
            return false;
        functions[i] = lbm_car(value);
        if (!closure_arity(functions[i], i == 1 ? 1 : 0))
            return false;
        value = lbm_cdr(value);
    }
    return value == ENC_SYM_NIL;
}
static lbm_value app_run(lbm_value *arguments, lbm_uint count)
{
    lbm_value functions[3];
    if (count != 1 || !descriptor(arguments[0], functions))
        return ENC_SYM_TERROR;
    if (!attached || lisp_runtime_app_active() || lisp_runtime_load_active() ||
        display.state(display.user) != LISP_APP_DISPLAY_TERMINAL)
        return ENC_SYM_EERROR;
    /* Copy the descriptor, then root it in the global environment. Never
     * retain an unrooted closure in C across GC. Callbacks revalidate lookup. */
    lbm_value root = lbm_heap_allocate_list_init(3, functions[0], functions[1], functions[2]);
    if (!lbm_is_cons(root) || !lbm_define(root_name, root))
        return ENC_SYM_MERROR;
    state = APP_QUEUED;
    stop_requested = init_done = exit_started = failed = false;
    callback_active = callback_done = false;
    key_head = key_count = 0;
    return ENC_SYM_TRUE;
}
static lbm_value app_stop(lbm_value *arguments, lbm_uint count)
{
    (void)arguments;
    if (count != 0)
        return ENC_SYM_TERROR;
    lisp_runtime_app_stop();
    return ENC_SYM_TRUE;
}
static bool integer(lbm_value value, int64_t *number)
{
    lbm_type type = lbm_type_of(value) & LBM_PTR_TO_CONSTANT_MASK;
    switch (type) {
    case LBM_TYPE_U:
    case LBM_TYPE_U32:
    case LBM_TYPE_U64: {
        uint64_t n = lbm_dec_as_u64(value);
        if (n > INT_MAX)
            return false;
        *number = (int64_t)n;
        return true;
    }
    case LBM_TYPE_I:
    case LBM_TYPE_I32:
    case LBM_TYPE_I64:
        *number = lbm_dec_as_i64(value);
        return *number >= INT_MIN && *number <= INT_MAX;
    default:
        return false; /* No float/NaN-to-int conversion or char-as-coordinate. */
    }
}
static bool numbers(lbm_value *arguments, lbm_uint count, unsigned wanted, int values[5])
{
    if (count != wanted)
        return false;
    for (unsigned i = 0; i < wanted; i++) {
        int64_t number;
        if (!integer(arguments[i], &number))
            return false;
        values[i] = (int)number;
    }
    return values[wanted - 1] >= 0 && values[wanted - 1] <= UINT16_MAX;
}
static bool drawing(void)
{
    return callback_active && job != APP_EXIT && draw_pixels != NULL && !stop_requested;
}
static lbm_value gfx_clear(lbm_value *arguments, lbm_uint count)
{
    int values[5];
    if (!numbers(arguments, count, 1, values))
        return ENC_SYM_TERROR;
    if (!drawing())
        return ENC_SYM_EERROR;
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    lisp_perf_token_t timing = job == APP_STEP ? lisp_perf_begin() : (lisp_perf_token_t){0};
#endif
    lisp_app_clear(draw_pixels, (uint16_t)values[0]);
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    lisp_perf_end(LISP_PERF_CLEAR, timing);
#endif
    dirty = true;
    return ENC_SYM_TRUE;
}
static lbm_value gfx_pixel(lbm_value *arguments, lbm_uint count)
{
    int values[5];
    if (!numbers(arguments, count, 3, values))
        return ENC_SYM_TERROR;
    if (!drawing())
        return ENC_SYM_EERROR;
    lisp_app_pixel(draw_pixels, values[0], values[1], (uint16_t)values[2]);
    dirty = true;
    return ENC_SYM_TRUE;
}
static lbm_value gfx_line(lbm_value *arguments, lbm_uint count)
{
    int values[5];
    if (!numbers(arguments, count, 5, values))
        return ENC_SYM_TERROR;
    if (!drawing())
        return ENC_SYM_EERROR;
    lisp_app_line(draw_pixels, values[0], values[1], values[2], values[3], (uint16_t)values[4]);
    dirty = true;
    return ENC_SYM_TRUE;
}
static lbm_value gfx_rect(lbm_value *arguments, lbm_uint count)
{
    int values[5];
    if (!numbers(arguments, count, 5, values))
        return ENC_SYM_TERROR;
    if (!drawing())
        return ENC_SYM_EERROR;
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    lisp_perf_token_t timing = job == APP_STEP ? lisp_perf_begin() : (lisp_perf_token_t){0};
#endif
    lisp_app_rect(draw_pixels, values[0], values[1], values[2], values[3], (uint16_t)values[4]);
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    lisp_perf_end(LISP_PERF_RECT, timing);
#endif
    dirty = true;
    return ENC_SYM_TRUE;
}
static lbm_value gfx_text(lbm_value *arguments, lbm_uint count)
{
    int64_t x, y, color;
    char *text = NULL;
    size_t size = 0;
    if (count != 4 || !integer(arguments[0], &x) || !integer(arguments[1], &y) ||
        !integer(arguments[3], &color) || color < 0 || color > UINT16_MAX ||
        !lbm_dec_str_size(arguments[2], &text, &size) || !text || size == 0 || size > 129 ||
        text[size - 1] != 0)
        return ENC_SYM_TERROR;
    for (size_t i = 0; i + 1 < size; i++)
        if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126)
            return ENC_SYM_TERROR;
    if (!drawing())
        return ENC_SYM_EERROR;
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    lisp_perf_token_t timing = job == APP_STEP ? lisp_perf_begin() : (lisp_perf_token_t){0};
#endif
    lisp_app_text(draw_pixels, (int)x, (int)y, text, size - 1, (uint16_t)color);
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    lisp_perf_end(LISP_PERF_TEXT, timing);
#endif
    dirty = true;
    return ENC_SYM_TRUE;
}
static lbm_value gfx_present(lbm_value *arguments, lbm_uint count)
{
    (void)arguments;
    if (count != 0)
        return ENC_SYM_TERROR;
    if (!drawing())
        return ENC_SYM_EERROR;
    dirty = true; /* Transactional: publication happens only after callback success. */
    return ENC_SYM_TRUE;
}
bool lisp_app_runtime_register(lisp_output_fn callback, void *user)
{
    output = callback;
    output_user = user;
    return lbm_add_extension("app-run", app_run) && lbm_add_extension("app-stop", app_stop) &&
           lbm_add_extension("gfx-clear", gfx_clear) && lbm_add_extension("gfx-pixel", gfx_pixel) &&
           lbm_add_extension("gfx-line", gfx_line) && lbm_add_extension("gfx-rect", gfx_rect) &&
           lbm_add_extension("gfx-text", gfx_text) && lbm_add_extension("gfx-present", gfx_present);
}
void lisp_app_runtime_launch_finished(bool success)
{
    if (state == APP_QUEUED) {
        if (success)
            state = APP_STARTING;
        else {
            (void)lbm_undefine(root_name);
            state = APP_IDLE;
        }
    }
}
static bool start_callback(app_job_t kind, int event)
{
    lbm_uint symbol;
    lbm_value root, functions[3];
    /* All previous contexts have finished. The reserved binding is the GC root,
     * and is looked up AFTER collection, not cached as a C pointer/value. */
    (void)lbm_perform_gc();
    if (!lbm_get_symbol_by_name(root_name, &symbol) || !lbm_global_env_lookup(&root, lbm_enc_sym(symbol)) ||
        !descriptor(root, functions))
        return false;
    lbm_value quoted = lbm_heap_allocate_list_init(2, ENC_SYM_QUOTE, functions[(unsigned)kind]);
    if (!lbm_is_cons(quoted))
        return false;
    lbm_value args = kind == APP_STEP ? lbm_cons(lbm_enc_i(event), ENC_SYM_NIL) : ENC_SYM_NIL;
    if (kind == APP_STEP && !lbm_is_cons(args))
        return false;
    lbm_value call = lbm_cons(quoted, args);
    if (!lbm_is_cons(call))
        return false;
    lbm_value program = lbm_cons(call, ENC_SYM_NIL);
    if (!lbm_is_cons(program))
        return false;
    callback_id = lbm_create_ctx(program, ENC_SYM_NIL, 256, NULL);
    if (callback_id < 0)
        return false;
    job = kind;
    callback_active = true;
    callback_done = callback_error = callback_exit = false;
    started_ms = clock_ms;
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    callback_timing = kind == APP_STEP ? lisp_perf_begin() : (lisp_perf_token_t){0};
#endif
    dirty = false;
    lisp_runtime_limit_output(APP_LOG_BYTES);
    return true;
}
bool lisp_app_runtime_done(eval_context_t *context)
{
    if (!callback_active || context->id != callback_id)
        return false;
    callback_error = lbm_is_error(context->r);
    lbm_uint exit_symbol;
    callback_exit = lbm_get_symbol_by_name("exit", &exit_symbol) && context->r == lbm_enc_sym(exit_symbol);
    if (lisp_runtime_output_dropped())
        log_message("WARNING: app print output truncated\n");
    if (callback_error) {
        char text[512] = {0};
        log_message("ERROR: app callback: ");
        if (lbm_print_value(text, sizeof(text), context->r))
            log_message(text);
        else
            log_message("<result too large>");
        log_message("\n");
    }
    callback_active = false;
    callback_done = true;
    callback_id = -1;
    return true;
}
static void has_context(eval_context_t *context, void *found, void *unused)
{
    (void)context;
    (void)unused;
    *(bool *)found = true;
}
static void finish_callback(void)
{
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    lisp_perf_end(LISP_PERF_CALLBACK, callback_timing);
    callback_timing.active = false;
#endif
    callback_done = false;
    draw_pixels = NULL;
    bool remaining = false;
    lbm_all_ctxs_iterator(has_context, &remaining, NULL);
    if (remaining) {
        log_message("ERROR: detached contexts unsupported; RAM definitions cleared\n");
        if (!lisp_runtime_reset())
            log_message("ERROR: runtime reset failed\n");
        return;
    }
    if (callback_error) {
        failed = true;
        stop_requested = true;
    }
    if (job == APP_EXIT) {
        begin_restore();
        return;
    }
    if (job == APP_INIT && !callback_error)
        init_done = true;
    if (job == APP_STEP && callback_exit)
        stop_requested = true;
    if (!stop_requested && (job == APP_INIT || dirty)) {
        if (!display.publish(display.user)) {
            log_message("ERROR: application frame publication failed\n");
            failed = stop_requested = true;
        } else if (job == APP_INIT) {
            display.activate(display.user);
            state = APP_RUNNING;
        }
    }
    if (job == APP_INIT && callback_error)
        begin_restore();
    /* Spend only the remainder of this callback's frame budget. A late callback
     * starts the next frame when drawable, rebasing on that new start; no catch-up. */
    next_tick_ms = started_ms + APP_FRAME_MS;
}
void lisp_app_runtime_step(uint64_t now_ms)
{
    clock_ms = now_ms;
    if (state == APP_IDLE || state == APP_QUEUED)
        return;
    if (state == APP_RESTORING) {
        if (!restore_clock_started) {
            restore_started_ms = clock_ms;
            restore_clock_started = true;
        }
        lisp_app_display_state_t status = display.state(display.user);
        if (status == LISP_APP_DISPLAY_TERMINAL) {
            state = APP_IDLE;
            log_message(!lisp_runtime_ready() ? "ERROR: terminal restored; Lisp runtime unavailable\n"
                        : failed              ? "Application failed; terminal restored\n"
                                              : "Application stopped; terminal restored\n");
        } else if (status == LISP_APP_DISPLAY_FAULT ||
                   clock_ms - restore_started_ms >= APP_RESTORE_TIMEOUT_MS) {
            state = APP_IDLE;
            log_message(lisp_runtime_ready()
                            ? "ERROR: LCD restoration not confirmed; UART REPL available\n"
                            : "ERROR: LCD restoration not confirmed; Lisp runtime unavailable\n");
        }
        return;
    }
    if (!lisp_runtime_ready())
        return;
    if (callback_active) {
        if (clock_ms - started_ms >= APP_CALLBACK_TIMEOUT_MS) {
            log_message("ERROR: app callback timeout; RAM definitions cleared\n");
            if (!lisp_runtime_reset())
                log_message("ERROR: runtime reset failed\n");
            return;
        }
        wrover_lisp_timestamp_us = (uint32_t)(clock_ms * 1000);
        (void)lbm_eval_step(APP_STEP_BATCH);
        if (callback_done)
            finish_callback();
        return;
    }
    if (stop_requested) {
        if (init_done && !exit_started) {
            exit_started = true;
            if (start_callback(APP_EXIT, 0))
                return;
            failed = true;
            log_message("ERROR: cannot start app exit callback\n");
        }
        begin_restore();
        return;
    }
    if (display.state(display.user) == LISP_APP_DISPLAY_FAULT) {
        failed = true;
        begin_restore();
        return;
    }
    if (state == APP_STARTING) {
        if (!display.begin(display.user)) {
            failed = true;
            log_message("ERROR: application canvas unavailable\n");
            begin_restore();
            return;
        }
        draw_pixels = display.draw(display.user);
        if (!draw_pixels || !start_callback(APP_INIT, 0)) {
            failed = true;
            log_message("ERROR: cannot start app init callback\n");
            begin_restore();
        }
        return;
    }
    if (key_count == 0 && clock_ms < next_tick_ms)
        return;
    lisp_app_display_state_t display_status = display.state(display.user);
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    if (display_status != LISP_APP_DISPLAY_DRAWABLE && key_count == 0 && !backpressure_timing.active)
        backpressure_timing = lisp_perf_begin();
    if (display_status == LISP_APP_DISPLAY_DRAWABLE && backpressure_timing.active) {
        lisp_perf_end(LISP_PERF_BACKPRESSURE, backpressure_timing);
        backpressure_timing.active = false;
    }
#endif
    if (display_status != LISP_APP_DISPLAY_DRAWABLE)
        return;
    int event = -1; /* Tick; other values are raw 1..127 input bytes. */
    if (key_count) {
        event = keys[key_head];
        key_head = (key_head + 1) % APP_KEYS;
        key_count--;
    }
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    if (event == -1)
        lisp_perf_record(LISP_PERF_TICK_LATE, (clock_ms - next_tick_ms) * 1000);
    lisp_perf_token_t copy_timing = lisp_perf_begin();
#endif
    draw_pixels = display.draw(display.user);
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    lisp_perf_end(LISP_PERF_CANVAS_COPY, copy_timing);
#endif
    if (!draw_pixels || !start_callback(APP_STEP, event)) {
        failed = stop_requested = true;
        log_message("ERROR: cannot start app step callback\n");
    }
}
