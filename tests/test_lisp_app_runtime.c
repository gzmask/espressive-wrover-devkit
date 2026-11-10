#include "lcd_terminal.h"
#include "lisp_runtime.h"
#include "lisp_test_storage.h"
#include "repl_frontend.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint16_t pixels[2][LISP_APP_PIXELS];
static lisp_app_canvas_t canvas;
static lcd_terminal_t terminal;
static char log_text[65536];
static size_t used;
static uint64_t now;
static bool render = true;
static bool backend_begin(void *user)
{
    return lisp_app_canvas_begin(user);
}
static uint16_t *backend_draw(void *user)
{
    return lisp_app_canvas_draw(user);
}
static bool backend_publish(void *user)
{
    return lisp_app_canvas_publish(user);
}
static void backend_activate(void *user)
{
    lisp_app_canvas_activate(user);
}
static void backend_restore(void *user)
{
    lisp_app_canvas_restore(user);
}
static lisp_app_display_state_t backend_state(void *user)
{
    return lisp_app_canvas_state(user);
}
static const lisp_app_display_t backend = {&canvas,          backend_begin,   backend_draw, backend_publish,
                                           backend_activate, backend_restore, backend_state};
static void output(void *user, const char *text, size_t length)
{
    (void)user;
    assert(length < sizeof(log_text) - used);
    memcpy(log_text + used, text, length);
    used += length;
    log_text[used] = 0;
    /* Same foreground routing as the production owner. */
    if (!lisp_runtime_app_active())
        lcd_terminal_write(&terminal, text, length);
}
static void advance(unsigned milliseconds)
{
    for (unsigned i = 0; i < milliseconds; i++, now++) {
        lisp_runtime_step(now);
        if (render && now % 200 == 0) {
            lisp_app_frame_t frame;
            assert(lisp_app_canvas_acquire(&canvas, &frame));
            lisp_app_canvas_complete(&canvas, &frame, true);
        }
    }
}
static void evaluate(const char *source)
{
    used = 0;
    log_text[0] = 0;
    assert(lisp_runtime_submit(source, strlen(source), now));
    while (lisp_runtime_busy())
        advance(1);
}
static void launch(const char *init, const char *step, const char *exit_fn)
{
    char source[1024];
    int count =
        snprintf(source, sizeof(source), "(app-run (list (lambda () %s) (lambda (e) %s) (lambda () %s)))",
                 init, step, exit_fn);
    assert(count > 0 && (size_t)count < sizeof(source));
    evaluate(source);
}
static void stop(void)
{
    lisp_runtime_app_stop();
    for (unsigned i = 0; i < 6200 && lisp_runtime_app_active(); i++)
        advance(1);
    assert(!lisp_runtime_app_active());
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    void *storage = lisp_test_allocate(lisp_runtime_storage_size());
    assert(storage && lisp_runtime_init(storage, lisp_runtime_storage_size(), output, NULL));
    assert(lisp_app_canvas_init(&canvas, pixels[0], pixels[1], LISP_APP_PIXELS));
    lcd_terminal_init(&terminal);
    assert(lisp_runtime_app_attach(&backend));
    if (!strcmp(argv[1], "lifecycle")) {
        evaluate("(define twice (lambda (x) (* x 2)))");
        launch("(progn (gfx-clear 0) (gfx-rect 20 30 120 60 63488) (gfx-text 20 110 \"Hello LispBM\" 65535) "
               "(gfx-present))",
               "(if (= e 113) 'exit t)", "t");
        assert(lisp_runtime_app_active() && !lisp_runtime_busy()); /* Async launch ACK. */
        lcd_terminal_t saved = terminal;
        advance(1000);
        assert(canvas.enabled && pixels[canvas.front][30 * 320 + 20] == 63488);
        assert(!memcmp(&saved, &terminal, sizeof(saved)));
        assert(!lisp_runtime_submit("(+ 1 2)", 7, now));
        assert(lisp_runtime_app_key('q'));
        advance(1000);
        assert(!lisp_runtime_app_active() && !canvas.enabled);
        evaluate("(twice 21)");
        assert(strstr(log_text, "=> 42"));
    } else if (!strcmp(argv[1], "long")) {
        launch("(gfx-clear 31)", "t", "t");
        advance(12000); /* Lifetime is not the 5s request deadline. */
        assert(lisp_runtime_app_active() && !lisp_runtime_busy() && canvas.enabled);
        stop();
        assert(lisp_app_canvas_state(&canvas) == LISP_APP_DISPLAY_TERMINAL);
    } else if (!strcmp(argv[1], "init-error")) {
        launch("(progn (gfx-clear 31) (gfx-present) (missing))", "t", "t");
        advance(1000);
        assert(!lisp_runtime_app_active() && !canvas.enabled);
        assert(strstr(log_text, "ERROR:"));
    } else if (!strcmp(argv[1], "step-error")) {
        launch("(gfx-clear 31)", "(missing)", "t");
        advance(1000);
        assert(!lisp_runtime_app_active() && strstr(log_text, "ERROR:"));
    } else if (!strcmp(argv[1], "exit-error")) {
        launch("(gfx-clear 31)", "t", "(missing)");
        advance(400);
        stop();
        assert(strstr(log_text, "ERROR:"));
        evaluate("(+ 1 2)");
        assert(strstr(log_text, "=> 3"));
    } else if (!strcmp(argv[1], "timeout")) {
        evaluate("(define keep 42) (define forever (lambda () (forever)))");
        launch("(gfx-clear 0)", "(forever)", "t");
        advance(6500);
        assert(!lisp_runtime_app_active() && strstr(log_text, "timeout"));
        evaluate("keep");
        assert(strstr(log_text, "ERROR:"));
    } else if (!strcmp(argv[1], "background")) {
        evaluate("(define forever (lambda () (forever)))");
        launch("(gfx-clear 0)", "(spawn forever)", "t");
        advance(1000);
        assert(!lisp_runtime_app_active() && strstr(log_text, "detached contexts"));
    } else if (!strcmp(argv[1], "controls")) {
        launch("(gfx-clear 0)", "(if (= e 113) 'exit t)", "t");
        advance(400);
        assert(lisp_runtime_app_control(":key q", 6) == LISP_APP_COMMAND_OK);
        advance(600);
        assert(!lisp_runtime_app_active());
        assert(lisp_runtime_app_control(":key q", 6) == LISP_APP_COMMAND_INVALID);
        assert(lisp_runtime_app_control(":stop", 5) == LISP_APP_COMMAND_OK);
        assert(lisp_runtime_app_control("3", 1) == LISP_APP_COMMAND_NONE);
    } else if (!strcmp(argv[1], "backpressure")) {
        launch("(gfx-clear 0)", "(gfx-pixel 0 0 31)", "t");
        render = false;
        advance(1000);
        assert(lisp_runtime_app_active() && !lisp_runtime_busy());
        assert(canvas.pending >= 0 && canvas.enabled);
        assert(lisp_runtime_app_control(":stop", 5) == LISP_APP_COMMAND_OK);
        advance(500);
        assert(lisp_runtime_app_active()); /* Cannot claim terminal restored yet. */
        render = true;
        advance(500);
        assert(!lisp_runtime_app_active());
    } else if (!strcmp(argv[1], "validation")) {
        evaluate("(app-run (list 1 2 3))");
        assert(strstr(log_text, "ERROR:") && !lisp_runtime_app_active());
        evaluate("(app-run (list (lambda (x) t) (lambda (e) t) (lambda () t)))");
        assert(strstr(log_text, "ERROR:") && !lisp_runtime_app_active());
        evaluate("(gfx-clear 0)");
        assert(strstr(log_text, "ERROR:"));
        launch("(gfx-clear 65536)", "t", "t");
        advance(1000);
        assert(!lisp_runtime_app_active() && strstr(log_text, "ERROR:"));
    } else if (!strcmp(argv[1], "gc")) {
        launch("(gfx-clear 31)", "(progn (list 1 2 3 4 5) t)", "t");
        advance(12000);
        assert(lisp_runtime_app_active());
        stop();
        for (unsigned i = 0; i < 50; i++) {
            launch("(gfx-clear 31)", "t", "t");
            advance(300);
            stop();
        }
        evaluate("(+ 1 2)");
        assert(strstr(log_text, "=> 3"));
    } else if (!strcmp(argv[1], "root")) {
        launch("(gfx-clear 31)", "(undefine '__wrover_app)", "t");
        advance(1000);
        assert(!lisp_runtime_app_active() && strstr(log_text, "ERROR:"));
    } else if (!strcmp(argv[1], "reset")) {
        launch("(gfx-clear 31)", "t", "t");
        advance(400);
        assert(lisp_runtime_reset());
        advance(600);
        assert(!lisp_runtime_app_active());
        evaluate("(+ 1 2)");
        assert(strstr(log_text, "=> 3"));
    } else if (!strcmp(argv[1], "launch-error")) {
        evaluate("(app-run (list (lambda () t) (lambda (e) t) (lambda () t))) (missing)");
        assert(!lisp_runtime_app_active() && !canvas.session && strstr(log_text, "ERROR:"));
    } else if (!strcmp(argv[1], "exit-timeout")) {
        evaluate("(define forever (lambda () (forever)))");
        launch("(gfx-clear 0)", "t", "(forever)");
        advance(400);
        stop();
        assert(strstr(log_text, "timeout"));
        evaluate("(+ 1 2)");
        assert(strstr(log_text, "=> 3"));
    } else if (!strcmp(argv[1], "overflow")) {
        launch("(gfx-clear 0)", "t", "t");
        advance(400);
        for (unsigned i = 0; i < 16; i++)
            assert(lisp_runtime_app_key('a'));
        assert(!lisp_runtime_app_key('a'));
        advance(1000);
        assert(!lisp_runtime_app_active() && strstr(log_text, "queue full"));
    } else if (!strcmp(argv[1], "incremental")) {
        launch("(gfx-clear 0)", "(if (= e -1) t (gfx-pixel e 0 31))", "t");
        advance(400);
        assert(lisp_runtime_app_key(10));
        advance(400);
        assert(lisp_runtime_app_key(11));
        advance(400);
        assert(pixels[canvas.front][10] == 31 && pixels[canvas.front][11] == 31);
        stop();
    } else if (!strcmp(argv[1], "bad-numbers")) {
        launch("(gfx-line 1.0 0 2 0 31)", "t", "t");
        advance(1000);
        assert(!lisp_runtime_app_active() && strstr(log_text, "ERROR:"));
        launch("(gfx-pixel 18446744073709551615u64 0 31)", "t", "t");
        advance(1000);
        assert(!lisp_runtime_app_active() && strstr(log_text, "ERROR:"));
        launch("(gfx-text 0 0 [1 2 3] 31)", "t", "t");
        advance(1000);
        assert(!lisp_runtime_app_active() && strstr(log_text, "ERROR:"));
    } else if (!strcmp(argv[1], "cancel-start")) {
        evaluate("(define keep 42)");
        launch("(gfx-clear 0)", "t", "t");
        stop();
        assert(!canvas.session && !canvas.enabled);
        evaluate("keep");
        assert(strstr(log_text, "=> 42"));
    } else if (!strcmp(argv[1], "restore-timeout")) {
        launch("(gfx-clear 31)", "t", "t");
        advance(400);
        render = false;
        stop();
        assert(strstr(log_text, "LCD restoration not confirmed"));
        assert(lisp_app_canvas_state(&canvas) == LISP_APP_DISPLAY_WAIT);
        evaluate("(+ 1 2)");
        assert(strstr(log_text, "=> 3"));
        render = true;
        advance(400);
        assert(lisp_app_canvas_state(&canvas) == LISP_APP_DISPLAY_TERMINAL);
    } else if (!strcmp(argv[1], "display-fault")) {
        launch("(gfx-clear 31)", "t", "t");
        advance(400);
        render = false;
        lisp_app_frame_t frame;
        assert(lisp_app_canvas_acquire(&canvas, &frame));
        lisp_app_canvas_complete(&canvas, &frame, false);
        advance(400);
        assert(!lisp_runtime_app_active() && strstr(log_text, "LCD restoration not confirmed"));
        assert(lisp_app_canvas_state(&canvas) == LISP_APP_DISPLAY_FAULT);
        evaluate("(+ 1 2)");
        assert(strstr(log_text, "=> 3"));
        launch("(gfx-clear 0)", "t", "t");
        assert(!lisp_runtime_app_active() && strstr(log_text, "ERROR:"));
    } else if (!strcmp(argv[1], "uart-stop")) {
        evaluate("(define keep 42)");
        assert(!lisp_runtime_app_input(3)); /* Terminal Ctrl-C remains the frontend's responsibility. */
        launch("(gfx-clear 0)", "t", "t");
        advance(400);
        assert(lisp_runtime_app_input('a'));
        assert(lisp_runtime_app_input(3));
        assert(lisp_runtime_app_input('b')); /* Consumed, not reinterpreted as source after stop. */
        advance(800);
        assert(!lisp_runtime_app_active());
        evaluate("keep");
        assert(strstr(log_text, "=> 42"));
    } else if (!strcmp(argv[1], "demo")) {
        FILE *file = fopen(LISP_APP_DEMO_PATH, "rb");
        assert(file);
        char source[1024];
        size_t length = fread(source, 1, sizeof(source) - 1, file);
        assert(length > 0 && fgetc(file) == EOF);
        assert(fclose(file) == 0);
        source[length] = 0;
        repl_frontend_t input;
        repl_frontend_init(&input);
        unsigned submissions = 0;
        for (size_t i = 0; i < length; i++)
            submissions += repl_frontend_feed(&input, (uint8_t)source[i]) == REPL_READY;
        assert(submissions == 1 && input.ready && !input.overflowed);
        assert(!strncmp(input.source, "(define demo", 12));
        evaluate(input.source);
        assert(!lisp_runtime_app_active()); /* Load does not take the screen. */
        evaluate("(app-run demo)");
        advance(600);
        assert(canvas.enabled && pixels[canvas.front][40 * 320 + 20] == 63488);
        assert(lisp_runtime_app_key('q'));
        advance(600);
        assert(!lisp_runtime_app_active());
        evaluate("(app-run demo)"); /* Definition survived normal exit. */
        advance(400);
        stop();
    } else
        return 1;
    lisp_test_free(storage, lisp_runtime_storage_size());
    puts("PASS");
    return 0;
}
