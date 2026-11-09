#include "cJSON.h"
#include "wifi_scan_logic.h"
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
static const wifi_scan_ap_t ap = {.ssid = {'t', 'e', 's', 't'},
                                  .ssid_length = 4,
                                  .bssid = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff},
                                  .channel = 6,
                                  .rssi = -42,
                                  .security = WIFI_SCAN_AUTH_WPA2,
                                  .authmode_id = 3};
static void text_is(const cJSON *json, const char *key, const char *value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(json, key);
    CHECK(cJSON_IsString(item) && strcmp(item->valuestring, value) == 0);
}
static cJSON *parse(const char *buffer)
{
    cJSON *json = cJSON_ParseWithOpts(buffer, NULL, true);
    CHECK(json != NULL);
    return json;
}
static void initial(void)
{
    wifi_scan_snapshot_t s = wifi_scan_initial();
    CHECK(s.phase == WIFI_SCAN_OFF && s.completed_scans == 0 && s.last_error == 0);
    CHECK(strcmp(wifi_scan_phase_name(s.phase), "off") == 0);
    CHECK(wifi_scan_phase_name((wifi_scan_phase_t)99) == NULL);
    char buffer[256];
    CHECK(wifi_scan_format_status(&s, buffer, sizeof(buffer)));
    cJSON *json = parse(buffer);
    text_is(json, "state", "off");
    CHECK(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(json, "connected")));
    CHECK(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(json, "ip")));
    CHECK(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(json, "rssi")));
    CHECK(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(json, "last_error")));
    cJSON_Delete(json);
}
static void transitions(void)
{
    wifi_scan_snapshot_t s = wifi_scan_initial();
    CHECK(!wifi_scan_begin(&s));
    CHECK(wifi_scan_starting(&s) && s.phase == WIFI_SCAN_STARTING);
    CHECK(!wifi_scan_starting(&s));
    CHECK(wifi_scan_begin(&s) && s.phase == WIFI_SCAN_SCANNING);
    CHECK(!wifi_scan_begin(&s));
    CHECK(wifi_scan_finish(&s, 24, 16) && s.phase == WIFI_SCAN_IDLE);
    CHECK(s.completed_scans == 1 && s.ap_count == 24 && s.reported_count == 16);
    CHECK(!wifi_scan_finish(&s, 0, 0));
    CHECK(wifi_scan_begin(&s));
    CHECK(s.ap_count == 24); /* Previous completed scan remains visible during scanning. */
    CHECK(wifi_scan_finish(&s, 0, 0));
    CHECK(s.completed_scans == 2 && s.ap_count == 0);
    CHECK(wifi_scan_fail(&s, -7) && s.phase == WIFI_SCAN_ERROR && s.last_error == -7);
    CHECK(!wifi_scan_begin(&s) && !wifi_scan_starting(&s));
    char buffer[256];
    CHECK(wifi_scan_format_status(&s, buffer, sizeof(buffer)));
    cJSON *json = parse(buffer);
    CHECK(cJSON_GetObjectItemCaseSensitive(json, "last_error")->valueint == -7);
    cJSON_Delete(json);
}
static void invalid(void)
{
    CHECK(!wifi_scan_starting(NULL) && !wifi_scan_begin(NULL));
    CHECK(!wifi_scan_finish(NULL, 0, 0) && !wifi_scan_fail(NULL, 1));
    wifi_scan_snapshot_t s = wifi_scan_initial();
    CHECK(!wifi_scan_fail(&s, 1));
    CHECK(wifi_scan_starting(&s) && wifi_scan_begin(&s));
    wifi_scan_snapshot_t unchanged = s;
    CHECK(!wifi_scan_finish(&s, 1, 2));
    CHECK(!wifi_scan_finish(&s, 30, WIFI_SCAN_MAX_RECORDS + 1));
    CHECK(!wifi_scan_fail(&s, 0));
    CHECK(memcmp(&s, &unchanged, sizeof(s)) == 0);
    s.completed_scans = UINT32_MAX;
    CHECK(wifi_scan_finish(&s, UINT16_MAX, WIFI_SCAN_MAX_RECORDS));
    CHECK(s.completed_scans == UINT32_MAX);
    char buffer[512] = "old";
    CHECK(!wifi_scan_format_status(NULL, buffer, sizeof(buffer)) && buffer[0] == 0);
    CHECK(!wifi_scan_format_ap(NULL, buffer, sizeof(buffer)) && buffer[0] == 0);
    CHECK(!wifi_scan_format_ap(&ap, NULL, 512));
    wifi_scan_ap_t bad = ap;
    bad.ssid_length = 33;
    CHECK(!wifi_scan_format_ap(&bad, buffer, sizeof(buffer)));
    bad = ap;
    bad.channel = 0;
    CHECK(!wifi_scan_format_ap(&bad, buffer, sizeof(buffer)));
    bad.channel = 12;
    CHECK(!wifi_scan_format_ap(&bad, buffer, sizeof(buffer)));
    bad = ap;
    bad.security = (wifi_scan_security_t)99;
    CHECK(!wifi_scan_format_ap(&bad, buffer, sizeof(buffer)));
    s.phase = (wifi_scan_phase_t)99;
    CHECK(!wifi_scan_format_status(&s, buffer, sizeof(buffer)));
}
static void records(void)
{
    char buffer[512];
    CHECK(wifi_scan_format_ap(&ap, buffer, sizeof(buffer)));
    cJSON *json = parse(buffer);
    text_is(json, "ssid_hex", "74657374");
    text_is(json, "bssid_masked", "**:**:**:**:ee:ff");
    text_is(json, "security", "wpa2_psk");
    CHECK(cJSON_GetObjectItemCaseSensitive(json, "rssi")->valueint == -42);
    CHECK(cJSON_GetObjectItemCaseSensitive(json, "channel")->valueint == 6);
    CHECK(cJSON_GetObjectItemCaseSensitive(json, "password") == NULL);
    CHECK(strstr(buffer, "aa:bb:cc:dd") == NULL);
    cJSON_Delete(json);
    wifi_scan_ap_t hidden = ap;
    hidden.ssid_length = 0;
    CHECK(wifi_scan_format_ap(&hidden, buffer, sizeof(buffer)));
    json = parse(buffer);
    text_is(json, "ssid_hex", "");
    cJSON_Delete(json);
    for (int security = WIFI_SCAN_AUTH_UNKNOWN; security <= WIFI_SCAN_AUTH_OWE; security++) {
        hidden.security = (wifi_scan_security_t)security;
        CHECK(wifi_scan_format_ap(&hidden, buffer, sizeof(buffer)));
        json = parse(buffer);
        CHECK(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(json, "security")));
        cJSON_Delete(json);
    }
}
static void octets(void)
{
    wifi_scan_ap_t binary = ap;
    const uint8_t bytes[] = {0, 0x1b, '\n', '"', '\\', 0x7f, 0x80, 0xff};
    memcpy(binary.ssid, bytes, sizeof(bytes));
    binary.ssid_length = sizeof(bytes);
    char buffer[512];
    CHECK(wifi_scan_format_ap(&binary, buffer, sizeof(buffer)));
    cJSON *json = parse(buffer);
    text_is(json, "ssid_hex", "001b0a225c7f80ff");
    cJSON_Delete(json);
    CHECK(strchr(buffer, '\n') == NULL && strchr(buffer, 0x1b) == NULL);
    memset(binary.ssid, 0xff, sizeof(binary.ssid));
    binary.ssid_length = 32;
    CHECK(wifi_scan_format_ap(&binary, buffer, sizeof(buffer)));
    json = parse(buffer);
    CHECK(strlen(cJSON_GetObjectItemCaseSensitive(json, "ssid_hex")->valuestring) == 64);
    cJSON_Delete(json);
}
static void bounds(void)
{
    wifi_scan_snapshot_t s = wifi_scan_initial();
    char storage[514];
    for (size_t cap = 0; cap <= 512; cap++) {
        for (unsigned which = 0; which < 2; which++) {
            memset(storage, '#', sizeof(storage));
            bool ok = which == 0 ? wifi_scan_format_status(&s, storage + 1, cap)
                                 : wifi_scan_format_ap(&ap, storage + 1, cap);
            CHECK(storage[0] == '#' && storage[cap + 1] == '#');
            if (ok) {
                cJSON *json = parse(storage + 1);
                cJSON_Delete(json);
            } else if (cap != 0) {
                CHECK(storage[1] == 0);
            }
        }
    }
}
static size_t budget, outstanding;
static void *limited_malloc(size_t size)
{
    if (budget == 0)
        return NULL;
    budget--;
    void *p = malloc(size);
    if (p != NULL)
        outstanding++;
    return p;
}
static void limited_free(void *p)
{
    if (p != NULL) {
        CHECK(outstanding > 0);
        outstanding--;
        free(p);
    }
}
static void allocation(void)
{
    wifi_scan_snapshot_t s = wifi_scan_initial();
    for (unsigned which = 0; which < 2; which++) {
        bool succeeded = false;
        for (size_t limit = 0; limit < 80; limit++) {
            char buffer[512] = "stale";
            budget = limit;
            outstanding = 0;
            cJSON_Hooks hooks = {.malloc_fn = limited_malloc, .free_fn = limited_free};
            cJSON_InitHooks(&hooks);
            bool ok = which == 0 ? wifi_scan_format_status(&s, buffer, sizeof(buffer))
                                 : wifi_scan_format_ap(&ap, buffer, sizeof(buffer));
            CHECK(outstanding == 0);
            cJSON_InitHooks(NULL);
            if (ok) {
                succeeded = true;
                break;
            }
            CHECK(buffer[0] == 0);
        }
        CHECK(succeeded);
    }
}
int main(int argc, char **argv)
{
    const struct {
        const char *name;
        void (*run)(void);
    } cases[] = {{"initial", initial},      {"transitions", transitions}, {"invalid", invalid},
                 {"records", records},      {"octets", octets},           {"bounds", bounds},
                 {"allocation", allocation}};
    if (argc != 2)
        return 1;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        if (strcmp(argv[1], cases[i].name) == 0) {
            cases[i].run();
            puts("PASS");
            return 0;
        }
    return 1;
}
