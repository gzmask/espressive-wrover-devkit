#include "lisp_runtime.h"
#include "lisp_test_storage.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Real interpreter/loader/canvas; only the download and LCD clock are modeled. */
static uint16_t pixels[2][LISP_APP_PIXELS], latest[LISP_APP_PIXELS], expected[LISP_APP_PIXELS];
static lisp_app_canvas_t canvas;
static lisp_app_url_slot_t slot;
static char download[LISP_APP_URL_SOURCE_CAPACITY], log_text[32768];
static size_t used;
static unsigned published, requests;
static uint64_t now;

static bool display_begin(void *user)
{
    return lisp_app_canvas_begin(user);
}
static uint16_t *display_draw(void *user)
{
    return lisp_app_canvas_draw(user);
}
static bool display_publish(void *user)
{
    lisp_app_canvas_t *app_canvas = user;
    assert(app_canvas->draw >= 0);
    memcpy(latest, app_canvas->pixels[app_canvas->draw], sizeof(latest));
    published++;
    return lisp_app_canvas_publish(app_canvas);
}
static void display_activate(void *user)
{
    lisp_app_canvas_activate(user);
}
static void display_restore(void *user)
{
    lisp_app_canvas_restore(user);
}
static lisp_app_display_state_t display_state(void *user)
{
    return lisp_app_canvas_state(user);
}
static const lisp_app_display_t display = {&canvas,          display_begin,   display_draw, display_publish,
                                           display_activate, display_restore, display_state};
static bool request(void *user, const char *url, size_t length)
{
    (void)user;
    requests++;
    return lisp_app_url_slot_request(&slot, url, length);
}
static lisp_app_url_phase_t poll_download(void *user, const char **source, size_t *length)
{
    (void)user;
    return lisp_app_url_slot_poll(&slot, source, length);
}
static void cancel(void *user)
{
    (void)user;
    lisp_app_url_slot_cancel(&slot);
}
static void release(void *user)
{
    (void)user;
    lisp_app_url_slot_release(&slot);
}
static const lisp_app_url_backend_t download_backend = {NULL, request, poll_download, cancel, release};
static void output(void *user, const char *text, size_t length)
{
    (void)user;
    assert(length < sizeof(log_text) - used);
    memcpy(log_text + used, text, length);
    used += length;
    log_text[used] = 0;
}
static void advance(unsigned milliseconds)
{
    for (unsigned i = 0; i < milliseconds; i++, now++) {
        lisp_runtime_step(now);
        if (now % 100 == 0) {
            lisp_app_frame_t frame;
            assert(lisp_app_canvas_acquire(&canvas, &frame));
            lisp_app_canvas_complete(&canvas, &frame, true);
        }
    }
    if (strstr(log_text, "ERROR:"))
        fprintf(stderr, "%s", log_text);
    assert(!strstr(log_text, "ERROR:"));
}
static void evaluate(const char *source)
{
    used = 0;
    log_text[0] = 0;
    assert(lisp_runtime_submit(source, strlen(source), now));
    for (unsigned i = 0; i < 6000 && lisp_runtime_busy(); i++)
        advance(1);
    assert(!lisp_runtime_busy());
    assert(strstr(log_text, "=>"));
}
static void load_demo(void)
{
    FILE *file = fopen(TINYGL_DEMO_PATH, "rb");
    assert(file);
    size_t length = fread(download, 1, sizeof(download) - 1, file);
    assert(length > 1023 && length <= LISP_APP_URL_SOURCE_LIMIT && fgetc(file) == EOF);
    assert(fclose(file) == 0);
    for (size_t i = 0; i < length; i++)
        assert((download[i] >= 32 && download[i] <= 126) || download[i] == '\n' || download[i] == '\r' ||
               download[i] == '\t');
    download[length] = 0;
    evaluate("(app-load-url \"https://example.com/tinygl.lisp\")");
    assert(requests == 1 && !strcmp(lisp_runtime_load_status(), "downloading"));
    lisp_app_url_slot_finish(&slot, true, length);
    advance(1); /* Owner copies the source before releasing the download buffer. */
    assert(slot.phase == LISP_APP_URL_IDLE);
    memset(download, '?', sizeof(download));
    for (unsigned i = 0; i < 6000 && lisp_runtime_load_active(); i++)
        advance(1);
    assert(!lisp_runtime_load_active() && !strcmp(lisp_runtime_load_status(), "loaded"));
    assert(!lisp_runtime_app_active() && !canvas.session && !published);
}
static void next_frame(void)
{
    unsigned before = published;
    for (unsigned i = 0; i < 5000 && published == before; i++)
        advance(1);
    assert(published == before + 1 && lisp_runtime_app_active());
}
static void launch(void)
{
    evaluate("(app-run tinygl)");
    assert(lisp_runtime_app_active() && !lisp_runtime_busy());
    next_frame();
}
static void restored(void)
{
    for (unsigned i = 0; i < 6000 && lisp_runtime_app_active(); i++)
        advance(1);
    assert(!lisp_runtime_app_active() && !canvas.enabled);
    assert(lisp_app_canvas_state(&canvas) == LISP_APP_DISPLAY_TERMINAL);
}
static void verify_frame(unsigned phase, bool paused)
{
    /* Independent projection oracle; production rasterizer is already unit tested. */
    static const int vertices[8][3] = {{-40, -40, -40}, {40, -40, -40}, {40, 40, -40}, {-40, 40, -40},
                                       {-40, -40, 40},  {40, -40, 40},  {40, 40, 40},  {-40, 40, 40}};
    static const int edges[12][3] = {{0, 1, 2047},  {1, 2, 2047},  {2, 3, 2047},  {3, 0, 2047},
                                     {4, 5, 63519}, {5, 6, 63519}, {6, 7, 63519}, {7, 4, 63519},
                                     {0, 4, 65535}, {1, 5, 65535}, {2, 6, 65535}, {3, 7, 65535}};
    int points[8][2];
    double angle = (double)phase * (2.0 * acos(-1.0) / 32.0);
    int sine = (int)lround(sin(angle) * 1024.0), cosine = (int)lround(cos(angle) * 1024.0);
    for (unsigned i = 0; i < 8; i++) {
        int x = vertices[i][0], y = vertices[i][1], z = vertices[i][2];
        int rx = (x * cosine - z * sine) / 1024, rz = (x * sine + z * cosine) / 1024;
        int ry = (4 * y - 3 * rz) / 5, depth = 240 + (3 * y + 4 * rz) / 5;
        assert(depth >= 170);
        points[i][0] = 160 + 200 * rx / depth;
        points[i][1] = 124 + 200 * ry / depth;
        assert(points[i][0] >= 80 && points[i][0] < 240);
        assert(points[i][1] >= 40 && points[i][1] < 208);
    }
    lisp_app_clear(expected, 2);
    lisp_app_text(expected, 16, 12, "Tiny GL / software 3D", 21, 65535);
    lisp_app_text(expected, 16, 216, "p/SPACE pause / q exit", 22, 33808);
    for (unsigned i = 0; i < 12; i++) {
        const int *a = points[edges[i][0]], *b = points[edges[i][1]];
        lisp_app_line(expected, a[0], a[1], b[0], b[1], (uint16_t)edges[i][2]);
    }
    if (paused)
        lisp_app_text(expected, 240, 12, "PAUSED", 6, 65504);
    assert(!memcmp(latest, expected, sizeof(expected)));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    void *storage = lisp_test_allocate(lisp_runtime_storage_size());
    assert(storage && lisp_runtime_init(storage, lisp_runtime_storage_size(), output, NULL));
    assert(lisp_app_canvas_init(&canvas, pixels[0], pixels[1], LISP_APP_PIXELS));
    assert(lisp_runtime_app_attach(&display));
    assert(lisp_app_url_slot_init(&slot, download, sizeof(download)));
    assert(lisp_runtime_load_attach(&download_backend));
    load_demo();
    if (!strcmp(argv[1], "load")) {
        evaluate("(length tinygl)");
        assert(strstr(log_text, "=> 3"));
    } else if (!strcmp(argv[1], "render") || !strcmp(argv[1], "stress")) {
        launch();
        verify_frame(0, false);
        unsigned frames = !strcmp(argv[1], "stress") ? 256 : 32;
        for (unsigned phase = 1; phase <= frames; phase++) {
            next_frame();
            verify_frame(phase % 32, false);
        }
        lisp_runtime_app_stop();
        restored();
        evaluate("(+ 1 2)");
        assert(strstr(log_text, "=> 3"));
    } else if (!strcmp(argv[1], "controls")) {
        launch();
        assert(lisp_runtime_app_control(":key p", 6) == LISP_APP_COMMAND_OK);
        next_frame();
        verify_frame(0, true);
        unsigned before = published;
        advance(1500);
        assert(published == before);
        assert(lisp_runtime_app_key('x'));
        advance(500);
        assert(published == before); /* Unrelated keys do not advance the animation. */
        assert(lisp_runtime_app_key(' '));
        next_frame();
        verify_frame(0, false);
        next_frame();
        verify_frame(1, false);
        assert(lisp_runtime_app_control(":key q", 6) == LISP_APP_COMMAND_OK);
        restored();
    } else if (!strcmp(argv[1], "restart")) {
        evaluate("(define keep 42)");
        for (unsigned i = 0; i < 8; i++) {
            launch();
            verify_frame(0, false);
            next_frame();
            assert(lisp_runtime_app_key(' '));
            next_frame();
            verify_frame(1, true);
            if (i % 2 == 0)
                assert(lisp_runtime_app_input(3));
            else
                assert(lisp_runtime_app_control(":stop", 5) == LISP_APP_COMMAND_OK);
            restored();
            evaluate("keep");
            assert(strstr(log_text, "=> 42"));
        }
    } else
        return 1;
    lisp_test_free(storage, lisp_runtime_storage_size());
    puts("PASS");
    return 0;
}
