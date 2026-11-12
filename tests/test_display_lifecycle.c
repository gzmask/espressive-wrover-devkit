#include "display_lifecycle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static display_release_step_t seen[DISPLAY_RELEASE_STEPS];
static unsigned calls;
static int fail_at = -1;
static int release_step(void *user, display_release_step_t step)
{
    assert(user == seen);
    assert(calls < DISPLAY_RELEASE_STEPS);
    seen[calls++] = step;
    return (int)step == fail_at ? 123 : 0;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    display_lifecycle_t lcd;
    display_lifecycle_init(&lcd);
    assert(lcd.state == DISPLAY_STARTING && !lcd.sd_reserved);
    assert(!display_lifecycle_health(&lcd, true));
    assert(!display_lifecycle_request(&lcd, false, true));
    assert(!display_lifecycle_sd_acquire(&lcd));
    if (!strcmp(argv[1], "switch")) {
        display_lifecycle_complete(&lcd, DISPLAY_ON);
        assert(display_lifecycle_health(&lcd, true));
        assert(!display_lifecycle_health(&lcd, false));
        assert(!display_lifecycle_request(&lcd, false, false));
        assert(lcd.state == DISPLAY_ON);
        assert(display_lifecycle_request(&lcd, false, true));
        assert(lcd.state == DISPLAY_STOPPING);
        assert(!display_lifecycle_request(&lcd, true, true));
        assert(!display_lifecycle_sd_acquire(&lcd));
        display_lifecycle_complete(&lcd, DISPLAY_OFF);
        assert(display_lifecycle_health(&lcd, false));
        assert(display_lifecycle_request(&lcd, false, true));
        assert(display_lifecycle_sd_acquire(&lcd));
        assert(!display_lifecycle_sd_acquire(&lcd));
        assert(!display_lifecycle_request(&lcd, true, true));
        assert(display_lifecycle_sd_release(&lcd));
        assert(!display_lifecycle_sd_release(&lcd));
        for (unsigned i = 0; i < 50; i++) {
            assert(display_lifecycle_request(&lcd, true, true));
            assert(lcd.state == DISPLAY_STARTING);
            display_lifecycle_complete(&lcd, DISPLAY_ON);
            assert(display_lifecycle_request(&lcd, true, true));
            assert(display_lifecycle_request(&lcd, false, true));
            display_lifecycle_complete(&lcd, DISPLAY_OFF);
        }
    } else if (!strcmp(argv[1], "blocked")) {
        display_lifecycle_complete(&lcd, DISPLAY_SD_BLOCKED);
        assert(display_lifecycle_health(&lcd, false));
        assert(display_lifecycle_sd_acquire(&lcd));
        assert(!display_lifecycle_request(&lcd, true, true));
        assert(display_lifecycle_sd_release(&lcd));
        assert(display_lifecycle_request(&lcd, true, true));
        display_lifecycle_complete(&lcd, DISPLAY_SD_BLOCKED);
        assert(display_lifecycle_request(&lcd, false, true));
        assert(lcd.state == DISPLAY_OFF);
    } else if (!strcmp(argv[1], "fault")) {
        display_lifecycle_complete(&lcd, DISPLAY_FAULT);
        assert(!display_lifecycle_health(&lcd, true));
        assert(!display_lifecycle_request(&lcd, true, true));
        assert(!display_lifecycle_request(&lcd, false, true));
        assert(!display_lifecycle_sd_acquire(&lcd));
        display_lifecycle_complete(&lcd, DISPLAY_OFF);
        assert(lcd.state == DISPLAY_FAULT);
    } else if (!strcmp(argv[1], "release")) {
        display_release_ops_t ops = {seen, release_step};
        assert(display_lifecycle_shutdown(&ops, true) != 0 && calls == 0);
        assert(display_lifecycle_shutdown(NULL, false) != 0);
        for (int failure = 0; failure < DISPLAY_RELEASE_STEPS; failure++) {
            calls = 0;
            fail_at = failure;
            assert(display_lifecycle_shutdown(&ops, false) == 123);
            assert(calls == (unsigned)failure + 1);
            for (unsigned i = 0; i < calls; i++)
                assert((unsigned)seen[i] == i);
        }
        calls = 0;
        fail_at = -1;
        assert(display_lifecycle_shutdown(&ops, false) == 0);
        assert(calls == DISPLAY_RELEASE_STEPS);
    } else {
        return 1;
    }
    puts("PASS");
    return 0;
}
