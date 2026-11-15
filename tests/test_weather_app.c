#include "cJSON.h"
#include "lisp_runtime.h"
#include "lisp_test_storage.h"
#include "lispbm.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { BG = 2, WHITE = 65535, MUTED = 33808, YELLOW = 65504, RED = 63488 };
static uint16_t pixels[2][LISP_APP_PIXELS], latest[LISP_APP_PIXELS], expected[LISP_APP_PIXELS];
static lisp_app_canvas_t canvas;
static lisp_app_url_slot_t source_slot;
static lisp_http_slot_t http_slot;
static char source[LISP_APP_URL_SOURCE_CAPACITY], body[LISP_HTTP_BODY_MAX + 1];
static char log_text[32768], last_url[513];
static size_t used;
static unsigned frames, source_requests, http_calls, accepted;
static uint64_t now, accepted_ms;
static int64_t utc_shift;
static bool clock_set = true;
static lisp_http_error_t next_error;
static const int64_t epoch = INT64_C(1893479400); /* synthetic 2030-01-01 06:30 UTC */

static void output(void *u, const char *s, size_t n)
{
    (void)u;
    assert(used + n < sizeof(log_text));
    memcpy(log_text + used, s, n);
    used += n;
    log_text[used] = 0;
}
static int64_t utc(void *u)
{
    (void)u;
    return clock_set ? epoch + (int64_t)(now / 1000) + utc_shift : 0;
}
static bool begin(void *u)
{
    return lisp_app_canvas_begin(u);
}
static uint16_t *draw(void *u)
{
    return lisp_app_canvas_draw(u);
}
static bool publish(void *u)
{
    lisp_app_canvas_t *c = u;
    assert(c->draw >= 0);
    memcpy(latest, c->pixels[c->draw], sizeof(latest));
    frames++;
    return lisp_app_canvas_publish(c);
}
static void activate(void *u)
{
    lisp_app_canvas_activate(u);
}
static void restore(void *u)
{
    lisp_app_canvas_restore(u);
}
static lisp_app_display_state_t state(void *u)
{
    return lisp_app_canvas_state(u);
}
static const lisp_app_display_t display = {&canvas, begin, draw, publish, activate, restore, state};
static bool source_request(void *u, const char *url, size_t n)
{
    (void)u;
    source_requests++;
    return lisp_app_url_slot_request(&source_slot, url, n);
}
static lisp_app_url_phase_t source_poll(void *u, const char **s, size_t *n)
{
    (void)u;
    return lisp_app_url_slot_poll(&source_slot, s, n);
}
static void source_cancel(void *u)
{
    (void)u;
    lisp_app_url_slot_cancel(&source_slot);
}
static void source_release(void *u)
{
    (void)u;
    lisp_app_url_slot_release(&source_slot);
}
static const lisp_app_url_backend_t loader = {NULL, source_request, source_poll, source_cancel,
                                              source_release};
static lisp_http_error_t http_request(void *u, const char *url, size_t n, uint32_t *id)
{
    (void)u;
    http_calls++;
    if (next_error != LISP_HTTP_OK)
        return next_error;
    lisp_http_error_t e = lisp_http_slot_request(&http_slot, url, n, id);
    if (e == LISP_HTTP_OK) {
        assert(n < sizeof(last_url));
        memcpy(last_url, url, n);
        last_url[n] = 0;
        accepted++;
        accepted_ms = now;
    }
    return e;
}
static lisp_http_phase_t http_poll(void *u, uint32_t id, lisp_http_result_t *r)
{
    (void)u;
    return lisp_http_slot_poll(&http_slot, id, r);
}
static bool http_cancel(void *u, uint32_t id)
{
    (void)u;
    return lisp_http_slot_cancel(&http_slot, id);
}
static bool http_release(void *u, uint32_t id)
{
    (void)u;
    return lisp_http_slot_release(&http_slot, id);
}
static const lisp_http_backend_t http = {NULL, http_request, http_poll, http_cancel, http_release};

static lbm_value binding(const char *name)
{
    lbm_uint sym;
    lbm_value v;
    assert(lbm_get_symbol_by_name((char *)name, &sym) && lbm_global_env_lookup(&v, lbm_enc_sym(sym)));
    return v;
}
static bool named(lbm_value v, const char *name)
{
    lbm_uint sym;
    return lbm_get_symbol_by_name((char *)name, &sym) && v == lbm_enc_sym(sym);
}
static const char *text(lbm_value v)
{
    char *s = lbm_dec_str(v);
    assert(s);
    return s;
}
static lbm_value item(lbm_value v, unsigned i)
{
    while (i--) {
        assert(lbm_is_cons(v));
        v = lbm_cdr(v);
    }
    assert(lbm_is_cons(v));
    return lbm_car(v);
}
static void contexts(eval_context_t *ctx, void *found, void *unused)
{
    (void)ctx;
    (void)unused;
    *(bool *)found = true;
}
static void advance(unsigned ms)
{
    for (unsigned i = 0; i < ms; i++, now++) {
        lisp_runtime_step(now);
        if (now % 100 == 0) {
            lisp_app_frame_t f;
            assert(lisp_app_canvas_acquire(&canvas, &f));
            lisp_app_canvas_complete(&canvas, &f, true);
        }
    }
    if (strstr(log_text, "ERROR:"))
        fprintf(stderr, "%s", log_text);
    assert(!strstr(log_text, "ERROR:"));
}
static void idle(void)
{
    for (unsigned i = 0; i < 5000; i++) {
        bool live = false;
        lbm_all_ctxs_iterator(contexts, &live, NULL);
        if (!live)
            return;
        advance(1);
    }
    assert(false);
}
static void jump(uint64_t to)
{
    idle();
    assert(to >= now);
    now = to;
}
static void eval(const char *s, const char *want)
{
    used = 0;
    log_text[0] = 0;
    assert(lisp_runtime_submit(s, strlen(s), now));
    for (unsigned i = 0; i < 6000 && lisp_runtime_busy(); i++)
        advance(1);
    assert(!lisp_runtime_busy());
    if (want && !strstr(log_text, want))
        fprintf(stderr, "%s -> %s; wanted %s\n", s, log_text, want);
    assert(!want || strstr(log_text, want));
}
static void wait_frame(unsigned before)
{
    for (unsigned i = 0; i < 5000 && frames == before; i++)
        advance(1);
    assert(frames > before && lisp_runtime_app_active());
}
static void launch(bool configured, bool fahrenheit)
{
    if (configured)
        eval(fahrenheit ? "(weather-config 0 0 'fahrenheit)" : "(weather-config 0 0 'celsius)", "=> t");
    unsigned before = frames;
    eval("(app-run weather)", "=> t");
    wait_frame(before);
}
static void stop(void)
{
    assert(lisp_runtime_app_control(":key q", 6) == LISP_APP_COMMAND_OK);
    for (unsigned i = 0; i < 6000 && lisp_runtime_app_active(); i++)
        advance(1);
    assert(!lisp_runtime_app_active() && state(&canvas) == LISP_APP_DISPLAY_TERMINAL);
}
static void load_weather(void)
{
    FILE *file = fopen(WEATHER_SOURCE, "rb");
    assert(file && "weather.lisp must exist");
    size_t n = fread(source, 1, sizeof(source) - 1, file);
    assert(n && n <= LISP_APP_URL_SOURCE_LIMIT && fgetc(file) == EOF && fclose(file) == 0);
    for (size_t i = 0; i < n; i++)
        assert((source[i] >= 32 && source[i] <= 126) || source[i] == '\n');
    source[n] = 0;
    eval("(app-load-url \"https://example.com/weather.lisp\")", "=> t");
    lisp_app_url_slot_finish(&source_slot, true, n);
    for (unsigned i = 0; i < 6000 && lisp_runtime_load_active(); i++)
        advance(1);
    assert(!strcmp(lisp_runtime_load_status(), "loaded") && !lisp_runtime_app_active() && !http_calls &&
           !frames);
    memset(source, '?', sizeof(source));
}
static cJSON *field(cJSON *o, const char *name)
{
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, name);
    assert(v);
    return v;
}
static cJSON *fixture(int offset)
{
    FILE *f = fopen(WEATHER_FIXTURE, "rb");
    assert(f);
    size_t n = fread(body, 1, sizeof(body) - 1, f);
    assert(fgetc(f) == EOF && fclose(f) == 0);
    body[n] = 0;
    cJSON *root = cJSON_Parse(body);
    assert(root);
    int64_t current = utc(NULL);
    if (!current)
        current = epoch;
    int64_t day = (current + offset) / 86400 * 86400 - offset;
    cJSON_SetNumberValue(field(root, "utc_offset_seconds"), offset);
    cJSON_SetNumberValue(field(field(root, "current"), "time"), (double)current);
    cJSON_SetNumberValue(cJSON_GetArrayItem(field(field(root, "daily"), "time"), 0), (double)day);
    cJSON *times = field(field(root, "hourly"), "time");
    for (int i = 0; i < 24; i++)
        cJSON_SetNumberValue(cJSON_GetArrayItem(times, i), (double)(day + i * 3600));
    return root;
}
static void complete_text(const char *s, lisp_http_error_t error, int status)
{
    assert(http_slot.phase == LISP_HTTP_FETCHING && strlen(s) < sizeof(body));
    if (s != body)
        strcpy(body, s);
    lisp_http_slot_finish(&http_slot, (lisp_http_result_t){error, status, body, strlen(body)});
}
static void complete(cJSON *root)
{
    assert(cJSON_PrintPreallocated(root, body, sizeof(body), false));
    cJSON_Delete(root);
    complete_text(body, LISP_HTTP_OK, 200);
}
static void wait_status(const char *status, unsigned before)
{
    for (unsigned i = 0; i < 5000; i++) {
        bool live = false;
        lbm_all_ctxs_iterator(contexts, &live, NULL);
        if (named(binding("weather-status"), status) && frames > before && !live)
            return;
        advance(1);
    }
    fprintf(stderr, "weather status did not reach %s\n%s", status, log_text);
    assert(false);
}
static void good(void)
{
    unsigned before = frames;
    complete(fixture(0));
    wait_status("ready", before);
    assert(http_slot.phase == LISP_HTTP_IDLE);
}
static void refresh(void)
{
    jump(accepted_ms + 60001);
    unsigned n = accepted;
    assert(lisp_runtime_app_key('r'));
    for (unsigned i = 0; i < 5000 && accepted == n; i++)
        advance(1);
    assert(accepted == n + 1);
}
static void label(int x, int y, const char *s, int color)
{
    lisp_app_text(expected, x, y, s, strlen(s), (uint16_t)color);
}
static void digit(int x, int y, unsigned n)
{
    static const unsigned masks[] = {63, 6, 91, 79, 102, 109, 125, 7, 127, 111};
    static const int segments[7][4] = {{4, 0, 16, 4},  {20, 4, 4, 16}, {20, 24, 4, 16}, {4, 40, 16, 4},
                                       {0, 24, 4, 16}, {0, 4, 4, 16},  {4, 20, 16, 4}};
    for (unsigned i = 0; i < 7; i++)
        if (masks[n] & (1u << i))
            lisp_app_rect(expected, x + segments[i][0], y + segments[i][1], segments[i][2], segments[i][3],
                          WHITE);
}
static void reference(bool fahrenheit)
{
    lisp_app_clear(expected, BG);
    label(12, 8, "WEATHER", WHITE);
    label(228, 8, "2030-01-01", WHITE);
    label(12, 28, "Lat 0.000 Lon 0.000", MUTED);
    digit(12, 52, fahrenheit ? 5 : 1);
    digit(40, 52, fahrenheit ? 5 : 3);
    label(142, 82, fahrenheit ? "F" : "C", WHITE);
    label(180, 54, "OVERCAST", WHITE);
    label(180, 82, fahrenheit ? "Feels 50 F" : "Feels 10 C", MUTED);
    label(12, 112, fahrenheit ? "Low 46 / High 61 F" : "Low 8 / High 16 C", WHITE);
    lisp_app_line(expected, 12, 132, 308, 132, 16904);
    const int x[] = {12, 116, 220};
    const char *hours[] = {"07:00", "12:00", "18:00"};
    const char *c[] = {"15 C", "11 C", "8 C"};
    const char *f[] = {"59 F", "52 F", "46 F"};
    for (unsigned i = 0; i < 3; i++) {
        label(x[i], 142, hours[i], MUTED);
        label(x[i], 158, fahrenheit ? f[i] : c[i], WHITE);
        label(x[i], 176, i == 2 ? "Rain 80%" : "Rain 20%", i == 2 ? YELLOW : MUTED);
    }
    label(12, 196, "BRING AN UMBRELLA", YELLOW);
    label(12, 212, "As of 06:30  +0m", MUTED);
    label(12, 228, "Open-Meteo.com | r refresh | q exit", MUTED);
    if (memcmp(expected, latest, sizeof(expected))) {
        unsigned different = 0;
        for (unsigned i = 0; i < LISP_APP_PIXELS; i++)
            if (expected[i] != latest[i])
                different++;
        fprintf(stderr, "weather pixels differ: %u\n", different);
    }
    assert(!memcmp(expected, latest, sizeof(expected)));
}
static void status_line(const char *s)
{
    lisp_app_clear(expected, BG);
    label(12, 212, s, RED);
    assert(!memcmp(expected + 212 * 320, latest + 212 * 320, 12 * 320 * sizeof(uint16_t)));
}
static void preview(void)
{
    const char *path = getenv("WEATHER_PREVIEW");
    if (!path)
        return;
    FILE *file = fopen(path, "wbx");
    assert(file);
    unsigned char header[54] = {'B', 'M'};
    uint32_t fields[] = {54 + 320 * 240 * 3, 54, 40, 320, 240, 1u | (24u << 16)};
    const unsigned offsets[] = {2, 10, 14, 18, 22, 26};
    for (unsigned i = 0; i < 6; i++)
        for (unsigned j = 0; j < 4; j++)
            header[offsets[i] + j] = (unsigned char)(fields[i] >> (8 * j));
    assert(fwrite(header, 1, sizeof(header), file) == sizeof(header));
    for (int y = 239; y >= 0; y--)
        for (int x = 0; x < 320; x++) {
            uint16_t v = latest[y * 320 + x];
            unsigned char rgb[] = {(unsigned char)((v & 31) * 255 / 31),
                                   (unsigned char)(((v >> 5) & 63) * 255 / 63),
                                   (unsigned char)((v >> 11) * 255 / 31)};
            assert(fwrite(rgb, 1, sizeof(rgb), file) == sizeof(rgb));
        }
    assert(fclose(file) == 0);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    size_t n = lisp_runtime_storage_size();
    void *storage = lisp_test_allocate(n);
    assert(storage && lisp_runtime_init(storage, n, output, NULL));
    assert(lisp_app_canvas_init(&canvas, pixels[0], pixels[1], LISP_APP_PIXELS));
    assert(lisp_runtime_app_attach(&display));
    assert(lisp_app_url_slot_init(&source_slot, source, sizeof(source)));
    assert(lisp_runtime_load_attach(&loader));
    assert(lisp_http_slot_init(&http_slot, body, sizeof(body)));
    assert(lisp_runtime_http_attach(&http));
    assert(lisp_runtime_time_attach(utc, NULL));
    load_weather();
    const char *which = argv[1];
    if (!strcmp(which, "load")) {
        eval("(length weather)", "=> 3");
        eval("(weather-config 0 0 'celsius)", "=> t");
        assert(!http_calls);
    } else if (!strcmp(which, "config")) {
        const char *bad[] = {"(weather-config 91 0 'celsius)", "(weather-config 0 -181 'celsius)",
                             "(weather-config nil 0 'celsius)", "(weather-config 0 0 'kelvin)"};
        for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
            eval(bad[i], "=> nil");
        launch(false, false);
        assert(!http_calls && named(binding("weather-status"), "unconfigured"));
        stop();
        eval("(weather-config 90 -180 'fahrenheit)", "=> t");
        lbm_value settings = binding("weather-settings");
        eval("(weather-config 91 0 'celsius)", "=> nil");
        assert(binding("weather-settings") == settings);
        launch(false, false);
        assert(accepted == 1 && strstr(last_url, "latitude=90.0000&longitude=-180.0000"));
        stop();
        complete_text("late", LISP_HTTP_OK, 200);
    } else if (!strcmp(which, "offline")) {
        next_error = LISP_HTTP_UNAVAILABLE;
        launch(true, false);
        assert(!accepted && http_calls == 1 && named(binding("weather-status"), "error"));
        assert(!strcmp(text(binding("weather-error")), "unavailable"));
        assert(lisp_runtime_app_key('r'));
        advance(1000);
        assert(http_calls == 1);
        next_error = LISP_HTTP_OK;
        jump(lbm_dec_as_u64(binding("weather-manual-ms")) + 1);
        assert(lisp_runtime_app_key('r'));
        advance(1500);
        assert(accepted == 1);
        good();
        stop();
    } else if (!strcmp(which, "codes")) {
        eval("(weather-condition 0i64)", "\"CLEAR\"");
        eval("(weather-condition 61i64)", "\"RAIN\"");
        eval("(weather-condition 99i64)", "\"THUNDERSTORM\"");
        eval("(weather-condition 999i64)", "\"UNKNOWN\"");
        eval("(weather-condition nil)", "\"UNKNOWN\"");
    } else if (!strcmp(which, "dates")) {
        const int days[] = {0, 10957, 11016, 19723, 20088, 21243, 21915, 47481};
        for (unsigned i = 0; i < sizeof(days) / sizeof(days[0]); i++) {
            time_t stamp = (time_t)days[i] * 86400;
            struct tm *date = gmtime(&stamp);
            assert(date);
            char wanted[32], expression[64];
            assert(strftime(wanted, sizeof(wanted), "%Y-%m-%d", date));
            (void)snprintf(expression, sizeof(expression), "(weather-date %d)", days[i]);
            eval(expression, wanted);
        }
    } else {
        launch(true, !strcmp(which, "fahrenheit"));
        assert(accepted == 1 && named(binding("weather-status"), "loading"));
        assert(strstr(last_url, "https://api.open-meteo.com/v1/forecast?latitude=0.0000&longitude=0.0000"));
        assert(strstr(last_url, "timezone=auto") && strstr(last_url, "timeformat=unixtime") &&
               strstr(last_url, "forecast_days=1") && strstr(last_url, "temperature_unit=celsius"));
        if (!strcmp(which, "forecast") || !strcmp(which, "fahrenheit")) {
            good();
            reference(!strcmp(which, "fahrenheit"));
            preview();
            stop();
        } else if (!strcmp(which, "negative")) {
            cJSON *root = fixture(0);
            cJSON_SetNumberValue(field(field(root, "current"), "temperature_2m"), -12.5);
            unsigned before = frames;
            complete(root);
            wait_status("ready", before);
            stop();
            eval("(weather-temp-text (ix weather-data 3))", "\"-13\"");
            eval("(weather-config 0 0 'fahrenheit)", "=> t");
            eval("(weather-temp-text (ix weather-data 3))", "\"10\"");
        } else if (!strcmp(which, "quarantine")) {
            jump(accepted_ms + 36001);
            wait_status("error", frames);
            jump(accepted_ms + 1800001);
            advance(1500);
            assert(accepted == 1 && !strcmp(text(binding("weather-error")), "busy"));
            assert(http_slot.abandoned && http_slot.phase == LISP_HTTP_FETCHING);
            complete_text("late", LISP_HTTP_OK, 200);
            jump(now + 60001);
            assert(lisp_runtime_app_key('r'));
            advance(1500);
            assert(accepted == 2);
            good();
            stop();
        } else if (!strcmp(which, "restart")) {
            for (unsigned i = 0; i < 8; i++) {
                good();
                stop();
                if (i != 7) {
                    launch(true, false);
                    assert(accepted == i + 2);
                }
            }
        } else if (!strcmp(which, "future-rain")) {
            cJSON *root = fixture(0);
            cJSON *rain = field(field(root, "hourly"), "precipitation_probability");
            for (int i = 0; i < 24; i++)
                cJSON_SetNumberValue(cJSON_GetArrayItem(rain, i), i < 6 ? 100 : 10);
            unsigned before = frames;
            complete(root);
            wait_status("ready", before);
            assert(lbm_dec_as_i64(item(binding("weather-data"), 9)) == 10);
            stop();
        } else if (!strcmp(which, "offset-half")) {
            unsigned before = frames;
            complete(fixture(19800));
            wait_status("ready", before);
            assert(!strcmp(text(item(binding("weather-data"), 11)), "12:00"));
            stop();
        } else if (!strcmp(which, "refresh")) {
            good();
            uint64_t start = accepted_ms;
            jump(start + 1790000);
            advance(500);
            assert(accepted == 1);
            jump(start + 1800001);
            advance(1500);
            assert(accepted == 2 && named(binding("weather-status"), "loading"));
            good();
            unsigned count = frames;
            advance(2000);
            assert(frames == count);
            assert(lisp_runtime_app_key('r'));
            advance(1000);
            assert(accepted == 2);
            stop();
        } else if (!strcmp(which, "errors")) {
            good();
            lbm_value saved = binding("weather-data");
            refresh();
            unsigned before = frames;
            complete_text("", LISP_HTTP_STATUS, 503);
            wait_status("error", before);
            assert(binding("weather-data") == saved && !strcmp(text(binding("weather-error")), "HTTP 503"));
            assert(http_slot.phase == LISP_HTTP_IDLE);
            status_line("STALE: HTTP 503");
            refresh();
            good();
            assert(binding("weather-error") == ENC_SYM_NIL && binding("weather-data") != saved);
            stop();
            eval("(weather-stale)", "=> nil");
            next_error = LISP_HTTP_OFFLINE;
            launch(true, false);
            assert(named(binding("weather-status"), "error") &&
                   !strcmp(text(binding("weather-error")), "offline"));
            stop();
        } else if (!strcmp(which, "schema")) {
            good();
            lbm_value saved = binding("weather-data");
            for (unsigned i = 0; i < 13; i++) {
                refresh();
                unsigned before = frames;
                cJSON *root = fixture(0), *current = field(root, "current"), *hourly = field(root, "hourly");
                if (i == 0) {
                    cJSON_Delete(root);
                    complete_text("{bad", LISP_HTTP_OK, 200);
                } else {
                    if (i == 1)
                        cJSON_ReplaceItemInObjectCaseSensitive(root, "current", cJSON_CreateNull());
                    if (i == 2)
                        cJSON_ReplaceItemInObjectCaseSensitive(current, "temperature_2m",
                                                               cJSON_CreateString("warm"));
                    if (i == 3)
                        cJSON_SetNumberValue(
                            cJSON_GetArrayItem(field(hourly, "precipitation_probability"), 7), 101);
                    if (i == 4)
                        cJSON_DeleteItemFromArray(field(hourly, "time"), 5);
                    if (i == 5)
                        cJSON_SetNumberValue(field(current, "time"), (double)utc(NULL) - 3 * 3600);
                    if (i == 6)
                        cJSON_SetNumberValue(field(current, "time"), (double)utc(NULL) + 3600);
                    if (i == 7)
                        cJSON_ReplaceItemInObjectCaseSensitive(field(root, "current_units"), "temperature_2m",
                                                               cJSON_CreateString("F"));
                    if (i == 8)
                        cJSON_SetNumberValue(field(current, "time"), (double)utc(NULL) + 0.5);
                    if (i == 9)
                        cJSON_SetNumberValue(
                            cJSON_GetArrayItem(field(field(root, "daily"), "temperature_2m_min"), 0), 90);
                    if (i == 10)
                        cJSON_ReplaceItemInArray(field(field(root, "daily"), "temperature_2m_min"), 0,
                                                 cJSON_CreateNull());
                    if (i == 11)
                        cJSON_SetNumberValue(cJSON_GetArrayItem(field(field(root, "daily"), "time"), 0),
                                             (double)(epoch / 86400 * 86400 - 86400));
                    if (i == 12)
                        cJSON_ReplaceItemInObjectCaseSensitive(current, "weather_code",
                                                               cJSON_CreateBool(false));
                    complete(root);
                }
                wait_status("error", before);
                assert(binding("weather-data") == saved);
                assert(http_slot.phase == LISP_HTTP_IDLE);
            }
            stop();
        } else if (!strcmp(which, "nulls")) {
            cJSON *root = fixture(0);
            cJSON_ReplaceItemInObjectCaseSensitive(field(root, "current"), "apparent_temperature",
                                                   cJSON_CreateNull());
            cJSON_ReplaceItemInObjectCaseSensitive(field(root, "current"), "weather_code",
                                                   cJSON_CreateNull());
            cJSON_ReplaceItemInArray(field(field(root, "hourly"), "temperature_2m"), 7, cJSON_CreateNull());
            cJSON_ReplaceItemInArray(field(field(root, "hourly"), "precipitation_probability"), 18,
                                     cJSON_CreateNull());
            unsigned before = frames;
            complete(root);
            wait_status("ready", before);
            lbm_value data = binding("weather-data");
            assert(item(data, 4) == ENC_SYM_NIL && item(data, 9) == ENC_SYM_NIL);
            assert(item(item(item(data, 8), 0), 0) == ENC_SYM_NIL);
            stop();
        } else if (!strcmp(which, "timeout") || !strcmp(which, "cancel")) {
            uint64_t first = accepted_ms;
            if (!strcmp(which, "timeout")) {
                jump(first + 36001);
                unsigned before = frames;
                wait_status("error", before);
                assert(!strcmp(text(binding("weather-error")), "timeout"));
                assert(http_slot.abandoned && http_slot.phase == LISP_HTTP_FETCHING);
                complete_text("late", LISP_HTTP_OK, 200);
                assert(http_slot.phase == LISP_HTTP_IDLE);
                advance(2000);
                assert(accepted == 1);
                stop();
            } else {
                stop();
                assert(http_slot.abandoned && http_slot.cancelled);
                complete_text("late", LISP_HTTP_OK, 200);
                assert(http_slot.phase == LISP_HTTP_IDLE);
                eval("(length weather)", "=> 3");
            }
        } else if (!strcmp(which, "stale")) {
            good();
            jump(accepted_ms + 3601000);
            advance(1500);
            assert(lisp_runtime_app_active() && accepted == 2 && binding("weather-error") == ENC_SYM_NIL);
            status_line("STALE: age 60m");
            stop();
            eval("(weather-stale)", "=> t");
        } else if (!strcmp(which, "clock-back") || !strcmp(which, "clock-lost")) {
            good();
            if (!strcmp(which, "clock-back"))
                utc_shift = -3600;
            else
                clock_set = false;
            unsigned before = frames;
            wait_frame(before);
            status_line("STALE: clock/date mismatch");
            stop();
            eval("(weather-stale)", "=> t");
        } else if (!strcmp(which, "midnight")) {
            good();
            utc_shift = 18 * 3600;
            unsigned before = frames;
            wait_frame(before);
            assert(accepted == 1);
            status_line("STALE: clock/date mismatch");
            stop();
            eval("(weather-stale)", "=> t");
            assert(!strcmp(text(item(binding("weather-data"), 10)), "2030-01-01"));
        } else if (!strcmp(which, "offset")) {
            unsigned before = frames;
            complete(fixture(-28800));
            wait_status("ready", before);
            lbm_value data = binding("weather-data");
            assert(!strcmp(text(item(data, 10)), "2029-12-31"));
            assert(!strcmp(text(item(data, 11)), "22:30"));
            stop();
        } else if (!strcmp(which, "clock")) {
            clock_set = false;
            unsigned before = frames;
            complete(fixture(0));
            wait_status("error", before);
            assert(!strcmp(text(binding("weather-error")), "clock"));
            assert(binding("weather-data") == ENC_SYM_NIL);
            stop();
        } else if (!strcmp(which, "stress")) {
            for (unsigned i = 0; i < 32; i++) {
                good();
                assert(lisp_runtime_app_active());
                if (i != 31) {
                    jump(accepted_ms + 1800001);
                    advance(1500);
                    assert(accepted == i + 2);
                }
            }
            stop();
            eval("(length weather)", "=> 3");
        } else
            return 1;
    }
    lisp_test_free(storage, n);
    puts("PASS: offline Open-Meteo app, real LispBM and canvas, simulated HTTP only");
    return 0;
}
