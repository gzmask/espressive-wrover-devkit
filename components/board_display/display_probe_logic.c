#include "display_probe_logic.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

display_model_t display_probe_identify(const display_id_sample_t *a, const display_id_sample_t *b)
{
    if (a == NULL || b == NULL || memcmp(a->rdid, b->rdid, sizeof(a->rdid)) != 0 ||
        memcmp(a->id4, b->id4, sizeof(a->id4)) != 0) {
        return DISPLAY_UNKNOWN;
    }
    bool ili = a->id4[0] == 0x93 && a->id4[1] == 0x41;
    bool st = a->rdid[0] == 0x85 && a->rdid[1] == 0x85 && a->rdid[2] == 0x52;
    if (ili == st) {
        return DISPLAY_UNKNOWN;
    }
    return ili ? DISPLAY_ILI9341 : DISPLAY_ST7789;
}

size_t display_probe_plan(display_model_t hint, display_model_t out[2])
{
    if (out == NULL) {
        return 0;
    }
    out[0] = out[1] = DISPLAY_UNKNOWN;
    if (hint != DISPLAY_ILI9341) {
        return 0;
    }
    out[0] = DISPLAY_ILI9341;
    return 1;
}

const char *display_probe_model_name(display_model_t model)
{
    switch (model) {
    case DISPLAY_ILI9341:
        return "ILI9341";
    case DISPLAY_ST7789:
        return "ST7789";
    default:
        return "UNKNOWN";
    }
}

/* Compact 5x7 column glyphs for this diagnostic's uppercase labels. */
static const struct {
    char character;
    uint8_t columns[5];
} glyphs[] = {
    {'0', {0x3e, 0x51, 0x49, 0x45, 0x3e}},
    {'1', {0, 0x42, 0x7f, 0x40, 0}},
    {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
    {'3', {0x21, 0x41, 0x45, 0x4b, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7f, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
    {'6', {0x3c, 0x4a, 0x49, 0x49, 0x30}},
    {'7', {1, 0x71, 9, 5, 3}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {6, 0x49, 0x49, 0x29, 0x1e}},
    {'A', {0x7e, 0x11, 0x11, 0x11, 0x7e}},
    {'B', {0x7f, 0x49, 0x49, 0x49, 0x36}},
    {'C', {0x3e, 0x41, 0x41, 0x41, 0x22}},
    {'D', {0x7f, 0x41, 0x41, 0x22, 0x1c}},
    {'E', {0x7f, 0x49, 0x49, 0x49, 0x41}},
    {'F', {0x7f, 9, 9, 9, 1}},
    {'G', {0x3e, 0x41, 0x49, 0x49, 0x7a}},
    {'H', {0x7f, 8, 8, 8, 0x7f}},
    {'I', {0, 0x41, 0x7f, 0x41, 0}},
    {'J', {0x20, 0x40, 0x41, 0x3f, 1}},
    {'K', {0x7f, 8, 0x14, 0x22, 0x41}},
    {'L', {0x7f, 0x40, 0x40, 0x40, 0x40}},
    {'M', {0x7f, 2, 0x0c, 2, 0x7f}},
    {'N', {0x7f, 4, 8, 0x10, 0x7f}},
    {'O', {0x3e, 0x41, 0x41, 0x41, 0x3e}},
    {'P', {0x7f, 9, 9, 9, 6}},
    {'Q', {0x3e, 0x41, 0x51, 0x21, 0x5e}},
    {'R', {0x7f, 9, 0x19, 0x29, 0x46}},
    {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
    {'T', {1, 1, 0x7f, 1, 1}},
    {'U', {0x3f, 0x40, 0x40, 0x40, 0x3f}},
    {'V', {0x1f, 0x20, 0x40, 0x20, 0x1f}},
    {'W', {0x3f, 0x40, 0x38, 0x40, 0x3f}},
    {'X', {0x63, 0x14, 8, 0x14, 0x63}},
    {'Y', {7, 8, 0x70, 8, 7}},
    {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
    {':', {0, 0x36, 0x36, 0, 0}},
    {'-', {8, 8, 8, 8, 8}},
    {'.', {0, 0x60, 0x60, 0, 0}},
    {'/', {0x20, 0x10, 8, 4, 2}},
    {'?', {2, 1, 0x51, 9, 6}},
};

static uint8_t glyph_column(char character, unsigned column)
{
    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
        if (glyphs[i].character == character) {
            return glyphs[i].columns[column];
        }
    }
    return 0;
}

static void put_pixel(uint8_t *out, unsigned x, unsigned row, uint16_t color)
{
    size_t offset = ((size_t)row * DISPLAY_WIDTH + x) * 2;
    out[offset] = (uint8_t)(color >> 8);
    out[offset + 1] = (uint8_t)color;
}

static void draw_text(uint8_t *out, unsigned first_row, unsigned rows, unsigned left, unsigned top,
                      const char *text, uint16_t color)
{
    const unsigned scale = 2;
    for (size_t index = 0; index < 26 && text[index] != '\0'; index++) {
        unsigned x_start = left + (unsigned)index * 6 * scale;
        if (x_start + 5 * scale > DISPLAY_WIDTH - 2) {
            break;
        }
        for (unsigned x = 0; x < 5; x++) {
            uint8_t bits = glyph_column(text[index], x);
            for (unsigned y = 0; y < 7; y++) {
                if ((bits & (1U << y)) == 0) {
                    continue;
                }
                for (unsigned dy = 0; dy < scale; dy++) {
                    unsigned absolute_y = top + y * scale + dy;
                    if (absolute_y < first_row || absolute_y >= first_row + rows) {
                        continue;
                    }
                    for (unsigned dx = 0; dx < scale; dx++) {
                        put_pixel(out, x_start + x * scale + dx, absolute_y - first_row, color);
                    }
                }
            }
        }
    }
}

bool display_probe_render_strip(uint8_t *out, size_t capacity, unsigned first_row, unsigned rows,
                                const display_probe_view_t *view)
{
    if (out == NULL || view == NULL || view->firmware == NULL || rows == 0 || rows > DISPLAY_STRIP_ROWS ||
        first_row >= DISPLAY_HEIGHT || rows > DISPLAY_HEIGHT - first_row ||
        capacity < (size_t)DISPLAY_WIDTH * rows * 2 ||
        (view->model != DISPLAY_ILI9341 && view->model != DISPLAY_ST7789) ||
        wifi_scan_phase_name(view->wifi.phase) == NULL || !wifi_link_valid(&view->link)) {
        return false;
    }
    const uint16_t colors[] = {0xf800, 0x07e0, 0x001f, 0xffff};
    unsigned marker = 12 + (unsigned)(view->uptime_s % 36) * 8;
    for (unsigned row = 0; row < rows; row++) {
        unsigned y = first_row + row;
        for (unsigned x = 0; x < DISPLAY_WIDTH; x++) {
            uint16_t color = 0;
            if (x < 2 || x >= DISPLAY_WIDTH - 2 || y < 2 || y >= DISPLAY_HEIGHT - 2) {
                color = 0xffff;
            } else if (y >= 160 && y < 184) {
                color = colors[x / 80];
            } else if (y >= 216 && y < 228 && x >= marker && x < marker + 12) {
                color = 0xffff;
            }
            put_pixel(out, x, row, color);
        }
    }
    char line[48];
    draw_text(out, first_row, rows, 12, 10, "TL  WROVER RADIO NODE", 0xffff);
    (void)snprintf(line, sizeof(line), "%c: %s %s", view->model == DISPLAY_ILI9341 ? 'A' : 'B',
                   display_probe_model_name(view->model), view->id_match ? "ID MATCH" : "TRIAL");
    draw_text(out, first_row, rows, 12, 34, line, 0xffe0);
    draw_text(out, first_row, rows, 12, 58, view->firmware, 0xffff);
    (void)snprintf(line, sizeof(line), "UPTIME: %" PRIu64 " S", view->uptime_s);
    draw_text(out, first_row, rows, 12, 82, line, 0xffff);
    (void)snprintf(line, sizeof(line), "HEAP: %" PRIu32 " B", view->free_heap_bytes);
    draw_text(out, first_row, rows, 12, 106, line, 0xffff);
    if (view->link.phase != WIFI_LINK_OFF) {
        if (view->link.rssi_valid)
            (void)snprintf(line, sizeof(line), "WIFI: %s %d DBM", wifi_link_phase_name(view->link.phase),
                           (int)view->link.rssi);
        else
            (void)snprintf(line, sizeof(line), "WIFI: %s", wifi_link_phase_name(view->link.phase));
        draw_text(out, first_row, rows, 12, 130, line, 0xffff);
        char ip[16];
        if (wifi_link_format_ip(&view->link, ip, sizeof(ip)))
            (void)snprintf(line, sizeof(line), "IP:%s", ip);
        else
            (void)snprintf(line, sizeof(line), "IP:-- TRY:%" PRIu32, view->link.attempts);
        draw_text(out, first_row, rows, 12, 146, line, 0xffff);
    } else {
        (void)snprintf(line, sizeof(line), "WIFI: %s  IP: --", wifi_scan_phase_name(view->wifi.phase));
        draw_text(out, first_row, rows, 12, 130, line, 0xffff);
    }
    if (view->link.phase == WIFI_LINK_OFF && view->wifi.phase != WIFI_SCAN_OFF) {
        (void)snprintf(line, sizeof(line), "AP:%u SCANS:%" PRIu32, (unsigned)view->wifi.ap_count,
                       view->wifi.completed_scans);
        draw_text(out, first_row, rows, 12, 146, line, 0xffff);
    }
    draw_text(out, first_row, rows, 30, 190, "R     G     B     W", 0xffff);
    draw_text(out, first_row, rows, 282, 218, "BR", 0xffe0);
    return true;
}
