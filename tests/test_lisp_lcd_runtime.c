#include "lisp_runtime.h"
#include "lisp_test_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static display_lifecycle_t lcd;
static char output[8192];
static size_t used;
static unsigned requests;
static bool request(void *user, bool on)
{
    assert(user == &lcd);
    requests++;
    return display_lifecycle_request(&lcd, on, true);
}
static display_lifecycle_state_t status(void *user)
{
    return ((display_lifecycle_t *)user)->state;
}
static void emit(void *user, const char *text, size_t n)
{
    (void)user;
    assert(n < sizeof(output) - used);
    memcpy(output + used, text, n);
    used += n;
    output[used] = 0;
}
static void evaluate(const char *source)
{
    used = 0;
    output[0] = 0;
    assert(lisp_runtime_submit(source, strlen(source), 0));
    for (uint64_t now = 0; lisp_runtime_busy() && now < 6000; now++)
        lisp_runtime_step(now);
    assert(!lisp_runtime_busy());
}
int main(void)
{
    size_t size = lisp_runtime_storage_size();
    void *memory = lisp_test_allocate(size);
    assert(lisp_runtime_init(memory, size, emit, NULL));
    evaluate("(lcd-status)");
    assert(strstr(output, "=> disabled"));
    evaluate("(lcd-off)");
    assert(strstr(output, "ERROR"));
    display_lifecycle_init(&lcd);
    display_lifecycle_complete(&lcd, DISPLAY_ON);
    display_lifecycle_backend_t backend = {&lcd, request, status};
    assert(!lisp_runtime_lcd_attach(NULL));
    assert(lisp_runtime_lcd_attach(&backend));
    evaluate("(define saved 42) (lcd-status)");
    assert(strstr(output, "=> on"));
    evaluate("(lcd-off 1)");
    assert(strstr(output, "ERROR") && requests == 0);
    evaluate("(lcd-on 1)");
    assert(strstr(output, "ERROR") && requests == 0);
    evaluate("(lcd-status 1)");
    assert(strstr(output, "ERROR") && requests == 0);
    evaluate("(lcd-off)");
    assert(strstr(output, "=> t") && lcd.state == DISPLAY_STOPPING);
    evaluate("(lcd-status)");
    assert(strstr(output, "=> stopping"));
    evaluate("(lcd-on)");
    assert(strstr(output, "ERROR") && lcd.state == DISPLAY_STOPPING);
    display_lifecycle_complete(&lcd, DISPLAY_OFF);
    evaluate("(lcd-status)");
    assert(strstr(output, "=> off"));
    evaluate("saved");
    assert(strstr(output, "=> 42"));
    assert(lisp_runtime_reset());
    evaluate("(lcd-status)");
    assert(strstr(output, "=> off"));
    assert(display_lifecycle_sd_acquire(&lcd));
    evaluate("(lcd-on)");
    assert(strstr(output, "ERROR") && lcd.state == DISPLAY_OFF);
    assert(display_lifecycle_sd_release(&lcd));
    evaluate("(lcd-on)");
    assert(strstr(output, "=> t") && lcd.state == DISPLAY_STARTING);
    display_lifecycle_complete(&lcd, DISPLAY_SD_BLOCKED);
    evaluate("(lcd-status)");
    assert(strstr(output, "=> sd-blocked"));
    evaluate("(lcd-on)");
    assert(lcd.state == DISPLAY_STARTING);
    display_lifecycle_complete(&lcd, DISPLAY_ON);
    evaluate("(lcd-status)");
    assert(strstr(output, "=> on"));
    lcd.state = DISPLAY_FAULT;
    evaluate("(lcd-off)");
    assert(strstr(output, "ERROR") && lcd.state == DISPLAY_FAULT);
    evaluate("(lcd-status)");
    assert(strstr(output, "=> fault"));
    lisp_test_free(memory, size);
    puts("PASS");
    return 0;
}
