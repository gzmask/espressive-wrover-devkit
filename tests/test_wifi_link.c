#include "cJSON.h"
#include "wifi_link_logic.h"
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
static const uint8_t ip[4] = {192, 168, 4, 23};
static wifi_link_snapshot_t connecting(void)
{
    wifi_link_snapshot_t s = wifi_link_initial();
    CHECK(wifi_link_configure(&s, 100));
    CHECK(wifi_link_begin(&s, 200));
    return s;
}
static wifi_link_snapshot_t online(void)
{
    wifi_link_snapshot_t s = connecting();
    CHECK(wifi_link_associated(&s, 6, 300));
    CHECK(wifi_link_ipv4(&s, ip, 400));
    return s;
}
static void initial(void)
{
    wifi_link_snapshot_t s = wifi_link_initial();
    CHECK(wifi_link_valid(&s) && s.phase == WIFI_LINK_OFF);
    CHECK(!wifi_link_begin(&s, 0));
    CHECK(!wifi_link_due(&s, UINT64_MAX));
    CHECK(wifi_link_phase_name((wifi_link_phase_t)99) == NULL);
    CHECK(!wifi_link_valid(NULL));
    CHECK(!wifi_link_configure(NULL, 0));
}
static void lifecycle(void)
{
    wifi_link_snapshot_t s = connecting();
    CHECK(s.attempts == 1 && s.phase == WIFI_LINK_CONNECTING && s.deadline_ms == 25200);
    CHECK(!wifi_link_due(&s, 25199) && wifi_link_due(&s, 25200));
    CHECK(!wifi_link_ipv4(&s, ip, 250));
    CHECK(wifi_link_associated(&s, 6, 300) && s.phase == WIFI_LINK_DHCP);
    CHECK(!wifi_link_signal(&s, -40));
    CHECK(wifi_link_ipv4(&s, ip, 400) && s.phase == WIFI_LINK_ONLINE);
    CHECK(wifi_link_signal(&s, -40) && s.rssi_valid);
    CHECK(!wifi_link_due(&s, UINT64_MAX));
    CHECK(wifi_link_failed(&s, 201, 0, 1000));
    CHECK(s.phase == WIFI_LINK_BACKOFF && !s.rssi_valid && s.channel == 0);
    CHECK(memcmp(s.ipv4, (uint8_t[4]){0}, 4) == 0);
    CHECK(!wifi_link_ipv4(&s, ip, 1001));
    CHECK(!wifi_link_begin(&s, 1999));
    CHECK(wifi_link_begin(&s, 2000) && s.attempts == 2);
}
static void backoff(void)
{
    wifi_link_snapshot_t s = connecting();
    uint64_t now = 300;
    const uint32_t delays[] = {1000, 2000, 4000, 8000, 16000, 30000};
    CHECK(wifi_link_backoff_ms(0) == 0);
    CHECK(wifi_link_backoff_ms(UINT32_MAX) == 30000);
    for (size_t i = 0; i < 6; i++) {
        CHECK(wifi_link_failed(&s, 202, 0, now));
        CHECK(s.phase == WIFI_LINK_BACKOFF && s.failures == i + 1);
        CHECK(s.deadline_ms - now == delays[i]);
        now = s.deadline_ms;
        CHECK(wifi_link_begin(&s, now));
        now++;
    }
    CHECK(wifi_link_failed(&s, 202, 0, now) && s.phase == WIFI_LINK_ERROR);
    CHECK(!wifi_link_begin(&s, UINT64_MAX));
    CHECK(!wifi_link_configure(&s, now));
}
static void flapping(void)
{
    wifi_link_snapshot_t s = online();
    CHECK(wifi_link_failed(&s, 200, 0, 500));
    CHECK(wifi_link_begin(&s, 1500));
    CHECK(wifi_link_associated(&s, 1, 1600));
    CHECK(wifi_link_ipv4(&s, ip, 1700));
    CHECK(s.failures == 1);
    CHECK(!wifi_link_stable(&s, 61699) && s.failures == 1);
    CHECK(wifi_link_stable(&s, 61700) && s.failures == 0);
    CHECK(wifi_link_failed(&s, 200, 0, 61800));
    CHECK(s.failures == 1 && s.deadline_ms == 62800);
}
static void invalid(void)
{
    wifi_link_snapshot_t s = connecting(), old = s;
    CHECK(!wifi_link_associated(&s, 0, 300));
    CHECK(!wifi_link_associated(&s, 12, 300));
    CHECK(!wifi_link_associated(&s, 1, 199));
    CHECK(memcmp(&s, &old, sizeof(s)) == 0);
    CHECK(!wifi_link_failed(&s, 0, 0, 400));
    CHECK(!wifi_link_fatal(&s, 0, 400));
    CHECK(wifi_link_associated(&s, 11, 300));
    CHECK(!wifi_link_ipv4(&s, NULL, 400));
    const uint8_t bad[][4] = {{0}, {127, 0, 0, 1}, {224, 1, 1, 1}, {255, 255, 255, 255}};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
        CHECK(!wifi_link_ipv4(&s, bad[i], 400));
    CHECK(wifi_link_fatal(&s, -1, 400) && s.phase == WIFI_LINK_ERROR);
    s.phase = (wifi_link_phase_t)99;
    CHECK(!wifi_link_valid(&s));
    s = wifi_link_initial();
    s.ipv4[0] = 1;
    CHECK(!wifi_link_valid(&s));
}
static void limits(void)
{
    wifi_link_snapshot_t s = wifi_link_initial();
    CHECK(wifi_link_configure(&s, UINT64_MAX - 10));
    CHECK(s.deadline_ms == UINT64_MAX);
    CHECK(wifi_link_begin(&s, UINT64_MAX - 9));
    CHECK(s.deadline_ms == UINT64_MAX);
    s.attempts = UINT32_MAX;
    CHECK(wifi_link_failed(&s, 201, 0, UINT64_MAX - 8));
    CHECK(wifi_link_begin(&s, UINT64_MAX));
    CHECK(s.attempts == UINT32_MAX);
}
static void json(void)
{
    wifi_link_snapshot_t s = online();
    CHECK(wifi_link_signal(&s, -45));
    wifi_link_snapshot_t before = s;
    CHECK(!wifi_link_renew_ipv4(&s, (uint8_t[4]){0}));
    CHECK(memcmp(&s, &before, sizeof(s)) == 0);
    CHECK(wifi_link_renew_ipv4(&s, ip));
    CHECK(s.since_ms == before.since_ms);
    char buffer[512], addr[16];
    CHECK(wifi_link_format_ip(&s, addr, sizeof(addr)) && strcmp(addr, "192.168.4.23") == 0);
    CHECK(wifi_link_format_json(&s, 500, buffer, sizeof(buffer)));
    cJSON *j = cJSON_Parse(buffer);
    CHECK(j != NULL);
    CHECK(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j, "connected")));
    CHECK(strcmp(cJSON_GetObjectItemCaseSensitive(j, "country")->valuestring, "US") == 0);
    CHECK(strcmp(cJSON_GetObjectItemCaseSensitive(j, "ip")->valuestring, addr) == 0);
    CHECK(cJSON_GetObjectItemCaseSensitive(j, "password") == NULL);
    CHECK(cJSON_GetObjectItemCaseSensitive(j, "ssid") == NULL);
    cJSON_Delete(j);
    CHECK(wifi_link_failed(&s, 201, 0, 600));
    CHECK(wifi_link_format_json(&s, 601, buffer, sizeof(buffer)));
    j = cJSON_Parse(buffer);
    CHECK(j != NULL);
    CHECK(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(j, "connected")));
    CHECK(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(j, "ip")));
    CHECK(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(j, "rssi")));
    cJSON_Delete(j);
}
static void bounds(void)
{
    wifi_link_snapshot_t s = online();
    char b[514];
    for (size_t cap = 0; cap <= 512; cap++) {
        memset(b, '#', sizeof(b));
        bool ok = wifi_link_format_json(&s, 0, b + 1, cap);
        CHECK(b[0] == '#' && b[cap + 1] == '#');
        if (ok) {
            cJSON *j = cJSON_Parse(b + 1);
            CHECK(j != NULL);
            cJSON_Delete(j);
        } else if (cap)
            CHECK(b[1] == 0);
    }
    CHECK(!wifi_link_format_json(NULL, 0, b, sizeof(b)) && b[0] == 0);
    CHECK(!wifi_link_format_json(&s, 0, NULL, 512));
}
static size_t budget, outstanding;
static void *alloc(size_t n)
{
    if (!budget)
        return NULL;
    budget--;
    void *p = malloc(n);
    if (p)
        outstanding++;
    return p;
}
static void release(void *p)
{
    if (p) {
        CHECK(outstanding > 0);
        outstanding--;
        free(p);
    }
}
static void allocation(void)
{
    wifi_link_snapshot_t s = online();
    bool success = false;
    for (size_t i = 0; i < 120; i++) {
        budget = i;
        outstanding = 0;
        char b[512] = "old";
        cJSON_Hooks hooks = {.malloc_fn = alloc, .free_fn = release};
        cJSON_InitHooks(&hooks);
        bool ok = wifi_link_format_json(&s, 0, b, sizeof(b));
        CHECK(outstanding == 0);
        cJSON_InitHooks(NULL);
        if (ok) {
            success = true;
            break;
        }
        CHECK(b[0] == 0);
    }
    CHECK(success);
}
int main(int argc, char **argv)
{
    const struct {
        const char *name;
        void (*run)(void);
    } cases[] = {{"initial", initial},   {"lifecycle", lifecycle}, {"backoff", backoff},
                 {"flapping", flapping}, {"invalid", invalid},     {"limits", limits},
                 {"json", json},         {"bounds", bounds},       {"allocation", allocation}};
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
