#include "display_probe_logic.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        if (!(c)) {                                                                                          \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                          \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static const display_probe_view_t view = {.model = DISPLAY_ILI9341,
                                          .id_match = false,
                                          .uptime_s = 0,
                                          .free_heap_bytes = 300000,
                                          .firmware = "0.1.2-lcd-probe"};
static const display_id_sample_t ili = {{0, 0, 0}, {0x93, 0x41}};
static const display_id_sample_t st = {{0x85, 0x85, 0x52}, {0, 0}};

static void identify(void)
{
    CHECK(display_probe_identify(&ili, &ili) == DISPLAY_ILI9341);
    CHECK(display_probe_identify(&st, &st) == DISPLAY_ST7789);
    display_id_sample_t blank = {0};
    CHECK(display_probe_identify(&blank, &blank) == DISPLAY_UNKNOWN);
    memset(&blank, 0xff, sizeof(blank));
    CHECK(display_probe_identify(&blank, &blank) == DISPLAY_UNKNOWN);
    blank = (display_id_sample_t){{0x12, 0x34, 0x56}, {0x77, 0x88}};
    CHECK(display_probe_identify(&blank, &blank) == DISPLAY_UNKNOWN);
    CHECK(display_probe_identify(NULL, &ili) == DISPLAY_UNKNOWN);
    CHECK(display_probe_identify(&st, NULL) == DISPLAY_UNKNOWN);
    CHECK(strcmp(display_probe_model_name(DISPLAY_UNKNOWN), "UNKNOWN") == 0);
}
static void unstable(void)
{
    display_id_sample_t changed = ili;
    changed.id4[1] ^= 1;
    CHECK(display_probe_identify(&ili, &changed) == DISPLAY_UNKNOWN);
    changed = st;
    changed.rdid[2] ^= 1;
    CHECK(display_probe_identify(&st, &changed) == DISPLAY_UNKNOWN);
    display_id_sample_t conflict = st;
    memcpy(conflict.id4, ili.id4, sizeof(conflict.id4));
    CHECK(display_probe_identify(&conflict, &conflict) == DISPLAY_UNKNOWN);
}
static void plan(void)
{
    display_model_t out[2];
    CHECK(display_probe_plan(DISPLAY_UNKNOWN, out) == 0);
    CHECK(out[0] == DISPLAY_UNKNOWN && out[1] == DISPLAY_UNKNOWN);
    out[0] = out[1] = DISPLAY_ST7789;
    CHECK(display_probe_plan(DISPLAY_ST7789, out) == 0);
    CHECK(out[0] == DISPLAY_UNKNOWN && out[1] == DISPLAY_UNKNOWN);
    CHECK(display_probe_plan(DISPLAY_ILI9341, out) == 1 && out[0] == DISPLAY_ILI9341);
    CHECK(out[1] == DISPLAY_UNKNOWN);
    CHECK(display_probe_plan(DISPLAY_UNKNOWN, NULL) == 0);
    CHECK(display_probe_plan((display_model_t)99, out) == 0);
}
static uint16_t pixel(const uint8_t *bytes, unsigned x, unsigned y)
{
    size_t offset = ((size_t)y * DISPLAY_WIDTH + x) * 2;
    return (uint16_t)(((uint16_t)bytes[offset] << 8) | bytes[offset + 1]);
}
static void colors(void)
{
    uint8_t bytes[DISPLAY_WIDTH * DISPLAY_STRIP_ROWS * 2];
    CHECK(display_probe_render_strip(bytes, sizeof(bytes), 160, 16, &view));
    CHECK(pixel(bytes, 40, 10) == 0xf800);
    CHECK(pixel(bytes, 120, 10) == 0x07e0);
    CHECK(pixel(bytes, 200, 10) == 0x001f);
    CHECK(pixel(bytes, 280, 10) == 0xffff);
    CHECK(pixel(bytes, 0, 0) == 0xffff); // border
}
static void bounds(void)
{
    uint8_t guard[DISPLAY_WIDTH * DISPLAY_STRIP_ROWS * 2 + 2];
    memset(guard, 0xa5, sizeof(guard));
    CHECK(display_probe_render_strip(guard + 1, sizeof(guard) - 2, 0, 16, &view));
    CHECK(guard[0] == 0xa5 && guard[sizeof(guard) - 1] == 0xa5);
    CHECK(!display_probe_render_strip(guard, 1, 0, 16, &view));
    CHECK(!display_probe_render_strip(NULL, sizeof(guard), 0, 16, &view));
    CHECK(!display_probe_render_strip(guard, sizeof(guard), 239, 2, &view));
    CHECK(!display_probe_render_strip(guard, sizeof(guard), UINT_MAX, 16, &view));
    CHECK(!display_probe_render_strip(guard, sizeof(guard), 0, UINT_MAX, &view));
    CHECK(!display_probe_render_strip(guard, sizeof(guard), 0, 0, &view));
    CHECK(!display_probe_render_strip(guard, sizeof(guard), 0, 16, NULL));
    display_probe_view_t invalid = view;
    invalid.firmware = NULL;
    CHECK(!display_probe_render_strip(guard, sizeof(guard), 0, 16, &invalid));
    invalid = view;
    invalid.model = DISPLAY_UNKNOWN;
    CHECK(!display_probe_render_strip(guard, sizeof(guard), 0, 16, &invalid));
    CHECK(display_probe_render_strip(guard, sizeof(guard), 239, 1, &view));
}
static void motion(void)
{
    uint8_t a[DISPLAY_WIDTH * DISPLAY_STRIP_ROWS * 2], b[sizeof(a)];
    display_probe_view_t next = view;
    next.uptime_s = 20;
    CHECK(display_probe_render_strip(a, sizeof(a), 216, 16, &view));
    CHECK(display_probe_render_strip(b, sizeof(b), 216, 16, &next));
    CHECK(memcmp(a, b, sizeof(a)) != 0);
    CHECK(pixel(a, 12, 3) == 0xffff && pixel(b, 12, 3) == 0);
    next.uptime_s = UINT64_MAX;
    next.free_heap_bytes = UINT32_MAX;
    CHECK(display_probe_render_strip(b, sizeof(b), 64, 16, &next));
}
static void layout(void)
{
    uint8_t a[DISPLAY_WIDTH * DISPLAY_STRIP_ROWS * 2], b[sizeof(a)];
    CHECK(display_probe_render_strip(a, sizeof(a), 0, 16, &view));
    CHECK(pixel(a, 12, 10) == 0xffff); /* T top stroke, doubled pixels. */
    CHECK(pixel(a, 13, 11) == 0xffff);
    CHECK(pixel(a, 12, 12) == 0);
    CHECK(display_probe_render_strip(a, sizeof(a), 32, 16, &view));
    display_probe_view_t other = view;
    other.model = DISPLAY_ST7789;
    CHECK(display_probe_render_strip(b, sizeof(b), 32, 16, &other));
    CHECK(memcmp(a, b, sizeof(a)) != 0); /* A/B label and driver name differ. */
    other = view;
    other.id_match = true;
    CHECK(display_probe_render_strip(b, sizeof(b), 32, 16, &other));
    CHECK(memcmp(a, b, sizeof(a)) != 0); /* A trial is never labeled as an ID match. */
    char long_firmware[513];
    memset(long_firmware, 'x', sizeof(long_firmware) - 1);
    long_firmware[sizeof(long_firmware) - 1] = '\0';
    other.firmware = long_firmware;
    CHECK(display_probe_render_strip(b, sizeof(b), 48, 16, &other));
}
static void strips(void)
{
    uint8_t strip[DISPLAY_WIDTH * DISPLAY_STRIP_ROWS * 2];
    uint8_t row[DISPLAY_WIDTH * 2];
    for (unsigned y = 0; y < DISPLAY_HEIGHT; y += DISPLAY_STRIP_ROWS) {
        CHECK(display_probe_render_strip(strip, sizeof(strip), y, DISPLAY_STRIP_ROWS, &view));
        for (unsigned offset = 0; offset < DISPLAY_STRIP_ROWS; offset++) {
            CHECK(display_probe_render_strip(row, sizeof(row), y + offset, 1, &view));
            CHECK(memcmp(row, strip + offset * sizeof(row), sizeof(row)) == 0);
        }
    }
}
static void network(void)
{
    uint8_t off[DISPLAY_WIDTH * DISPLAY_STRIP_ROWS * 2], current[sizeof(off)], previous[sizeof(off)];
    CHECK(display_probe_render_strip(off, sizeof(off), 128, 16, &view));
    memcpy(previous, off, sizeof(off));
    for (int phase = WIFI_SCAN_STARTING; phase <= WIFI_SCAN_ERROR; phase++) {
        display_probe_view_t next = view;
        next.wifi.phase = (wifi_scan_phase_t)phase;
        CHECK(display_probe_render_strip(current, sizeof(current), 128, 16, &next));
        CHECK(memcmp(current, off, sizeof(off)) != 0);
        CHECK(memcmp(current, previous, sizeof(off)) != 0);
        memcpy(previous, current, sizeof(off));
    }
    display_probe_view_t next = view;
    next.wifi.phase = WIFI_SCAN_IDLE;
    CHECK(display_probe_render_strip(previous, sizeof(previous), 144, 16, &next));
    next.wifi.ap_count = UINT16_MAX;
    next.wifi.completed_scans = UINT32_MAX;
    CHECK(display_probe_render_strip(current, sizeof(current), 144, 16, &next));
    CHECK(memcmp(current, previous, sizeof(off)) != 0);
    next.wifi.phase = (wifi_scan_phase_t)99;
    CHECK(!display_probe_render_strip(current, sizeof(current), 128, 16, &next));
}
static void station(void)
{
    uint8_t a[DISPLAY_WIDTH * DISPLAY_STRIP_ROWS * 2], b[sizeof(a)];
    display_probe_view_t next = view;
    CHECK(display_probe_render_strip(a, sizeof(a), 128, 16, &next));
    CHECK(wifi_link_configure(&next.link, 0));
    CHECK(display_probe_render_strip(b, sizeof(b), 128, 16, &next));
    CHECK(memcmp(a, b, sizeof(a)) != 0);
    CHECK(wifi_link_begin(&next.link, 1));
    CHECK(wifi_link_associated(&next.link, 6, 2));
    const uint8_t ip[] = {192, 168, 4, 23};
    CHECK(wifi_link_ipv4(&next.link, ip, 3));
    CHECK(wifi_link_signal(&next.link, -45));
    CHECK(display_probe_render_strip(a, sizeof(a), 144, 16, &next));
    next.link.ipv4[3] = 24;
    CHECK(display_probe_render_strip(b, sizeof(b), 144, 16, &next));
    CHECK(memcmp(a, b, sizeof(a)) != 0);
    CHECK(wifi_link_failed(&next.link, 201, 0, 4));
    CHECK(display_probe_render_strip(a, sizeof(a), 144, 16, &next));
    CHECK(memcmp(a, b, sizeof(a)) != 0);
    CHECK(wifi_link_fatal(&next.link, -2, 5));
    CHECK(display_probe_render_strip(a, sizeof(a), 128, 16, &next));
    next.link.phase = (wifi_link_phase_t)99;
    CHECK(!display_probe_render_strip(b, sizeof(b), 128, 16, &next));
}
int main(int argc, char **argv)
{
    const struct {
        const char *name;
        void (*run)(void);
    } tests[] = {
        {"identify", identify}, {"unstable", unstable}, {"plan", plan},     {"colors", colors},
        {"bounds", bounds},     {"motion", motion},     {"layout", layout}, {"strips", strips},
        {"network", network},   {"station", station},
    };
    if (argc != 2)
        return 1;
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        if (strcmp(argv[1], tests[i].name) == 0) {
            tests[i].run();
            puts("PASS");
            return 0;
        }
    }
    return 1;
}
