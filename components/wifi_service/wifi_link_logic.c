#include "wifi_link_logic.h"
#include "cJSON.h"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static uint64_t deadline(uint64_t now, uint32_t delay)
{
    return UINT64_MAX - now < delay ? UINT64_MAX : now + delay;
}
static bool ip_valid(const uint8_t ip[4])
{
    return ip != NULL && ip[0] != 0 && ip[0] != 127 && ip[0] < 224;
}
static bool address_zero(const uint8_t ip[4])
{
    return (ip[0] | ip[1] | ip[2] | ip[3]) == 0;
}
static void clear_network(wifi_link_snapshot_t *s)
{
    memset(s->ipv4, 0, sizeof(s->ipv4));
    s->channel = 0;
    s->rssi = 0;
    s->rssi_valid = false;
}
wifi_link_snapshot_t wifi_link_initial(void)
{
    return (wifi_link_snapshot_t){.phase = WIFI_LINK_OFF};
}
const char *wifi_link_phase_name(wifi_link_phase_t phase)
{
    switch (phase) {
    case WIFI_LINK_OFF:
        return "off";
    case WIFI_LINK_CONFIG:
        return "config";
    case WIFI_LINK_CONNECTING:
        return "connecting";
    case WIFI_LINK_DHCP:
        return "dhcp";
    case WIFI_LINK_ONLINE:
        return "online";
    case WIFI_LINK_BACKOFF:
        return "backoff";
    case WIFI_LINK_ERROR:
        return "error";
    default:
        return NULL;
    }
}
bool wifi_link_valid(const wifi_link_snapshot_t *s)
{
    if (s == NULL || wifi_link_phase_name(s->phase) == NULL || s->failures > WIFI_LINK_MAX_FAILURES)
        return false;
    bool associated = s->phase == WIFI_LINK_DHCP || s->phase == WIFI_LINK_ONLINE;
    return (associated ? s->channel >= 1 && s->channel <= 11 : s->channel == 0) &&
           (s->phase == WIFI_LINK_ONLINE ? ip_valid(s->ipv4) : address_zero(s->ipv4)) &&
           (!s->rssi_valid || (s->phase == WIFI_LINK_ONLINE && s->rssi >= -127 && s->rssi <= 0)) &&
           (s->phase != WIFI_LINK_ERROR || s->last_error != 0);
}
bool wifi_link_configure(wifi_link_snapshot_t *s, uint64_t now)
{
    if (!wifi_link_valid(s) || s->phase != WIFI_LINK_OFF)
        return false;
    s->phase = WIFI_LINK_CONFIG;
    s->since_ms = now;
    s->deadline_ms = deadline(now, WIFI_LINK_CONFIG_MS);
    return true;
}
bool wifi_link_begin(wifi_link_snapshot_t *s, uint64_t now)
{
    if (!wifi_link_valid(s) || now < s->since_ms ||
        !((s->phase == WIFI_LINK_CONFIG && now < s->deadline_ms) ||
          (s->phase == WIFI_LINK_BACKOFF && now >= s->deadline_ms)))
        return false;
    s->phase = WIFI_LINK_CONNECTING;
    s->since_ms = now;
    s->deadline_ms = deadline(now, WIFI_LINK_CONNECT_MS);
    s->last_error = 0;
    if (s->attempts < UINT32_MAX)
        s->attempts++;
    return true;
}
bool wifi_link_associated(wifi_link_snapshot_t *s, uint8_t channel, uint64_t now)
{
    if (!wifi_link_valid(s) || s->phase != WIFI_LINK_CONNECTING || now < s->since_ms ||
        now >= s->deadline_ms || channel < 1 || channel > 11)
        return false;
    s->phase = WIFI_LINK_DHCP;
    s->channel = channel;
    s->since_ms = now;
    s->deadline_ms = deadline(now, WIFI_LINK_DHCP_MS);
    return true;
}
bool wifi_link_ipv4(wifi_link_snapshot_t *s, const uint8_t ip[4], uint64_t now)
{
    if (!wifi_link_valid(s) || s->phase != WIFI_LINK_DHCP || now < s->since_ms || now >= s->deadline_ms ||
        !ip_valid(ip))
        return false;
    memcpy(s->ipv4, ip, sizeof(s->ipv4));
    s->phase = WIFI_LINK_ONLINE;
    s->since_ms = now;
    s->deadline_ms = 0;
    s->last_error = 0;
    return true;
}
bool wifi_link_renew_ipv4(wifi_link_snapshot_t *s, const uint8_t ip[4])
{
    if (!wifi_link_valid(s) || s->phase != WIFI_LINK_ONLINE || !ip_valid(ip))
        return false;
    memcpy(s->ipv4, ip, sizeof(s->ipv4));
    return true;
}
uint32_t wifi_link_backoff_ms(uint32_t failures)
{
    if (failures == 0)
        return 0;
    if (failures >= 6)
        return 30000;
    return UINT32_C(1000) << (failures - 1);
}
bool wifi_link_fatal(wifi_link_snapshot_t *s, int32_t error, uint64_t now)
{
    if (!wifi_link_valid(s) || s->phase == WIFI_LINK_ERROR || error == 0 || now < s->since_ms)
        return false;
    clear_network(s);
    s->phase = WIFI_LINK_ERROR;
    s->last_error = error;
    s->since_ms = now;
    s->deadline_ms = 0;
    return true;
}
bool wifi_link_failed(wifi_link_snapshot_t *s, uint16_t reason, int32_t error, uint64_t now)
{
    if (!wifi_link_valid(s) || now < s->since_ms || (reason == 0 && error == 0) ||
        (s->phase != WIFI_LINK_CONNECTING && s->phase != WIFI_LINK_DHCP && s->phase != WIFI_LINK_ONLINE))
        return false;
    clear_network(s);
    s->disconnect_reason = reason;
    s->last_error = error;
    if (s->failures < WIFI_LINK_MAX_FAILURES)
        s->failures++;
    s->since_ms = now;
    if (s->failures >= WIFI_LINK_MAX_FAILURES) {
        s->phase = WIFI_LINK_ERROR;
        if (s->last_error == 0)
            s->last_error = -1; /* Retry budget exhausted. */
        s->deadline_ms = 0;
    } else {
        s->phase = WIFI_LINK_BACKOFF;
        s->deadline_ms = deadline(now, wifi_link_backoff_ms(s->failures));
    }
    return true;
}
bool wifi_link_signal(wifi_link_snapshot_t *s, int8_t rssi)
{
    if (!wifi_link_valid(s) || s->phase != WIFI_LINK_ONLINE || rssi < -127 || rssi > 0)
        return false;
    s->rssi = rssi;
    s->rssi_valid = true;
    return true;
}
bool wifi_link_stable(wifi_link_snapshot_t *s, uint64_t now)
{
    if (!wifi_link_valid(s) || s->phase != WIFI_LINK_ONLINE || s->failures == 0 || now < s->since_ms ||
        now - s->since_ms < WIFI_LINK_STABLE_MS)
        return false;
    s->failures = 0;
    return true;
}
bool wifi_link_due(const wifi_link_snapshot_t *s, uint64_t now)
{
    return wifi_link_valid(s) && now >= s->deadline_ms &&
           (s->phase == WIFI_LINK_CONFIG || s->phase == WIFI_LINK_CONNECTING || s->phase == WIFI_LINK_DHCP ||
            s->phase == WIFI_LINK_BACKOFF);
}
bool wifi_link_format_ip(const wifi_link_snapshot_t *s, char *out, size_t capacity)
{
    if (out == NULL || capacity == 0)
        return false;
    out[0] = 0;
    if (!wifi_link_valid(s) || s->phase != WIFI_LINK_ONLINE || capacity < 16)
        return false;
    int n = snprintf(out, capacity, "%u.%u.%u.%u", (unsigned)s->ipv4[0], (unsigned)s->ipv4[1],
                     (unsigned)s->ipv4[2], (unsigned)s->ipv4[3]);
    return n > 0 && (size_t)n < capacity;
}
bool wifi_link_format_json(const wifi_link_snapshot_t *s, uint64_t now, char *out, size_t capacity)
{
    if (out == NULL || capacity == 0)
        return false;
    out[0] = 0;
    if (capacity > INT_MAX || !wifi_link_valid(s))
        return false;
    cJSON *j = cJSON_CreateObject();
    if (j == NULL)
        return false;
    bool online = s->phase == WIFI_LINK_ONLINE;
    char ip[16];
    if (online && !wifi_link_format_ip(s, ip, sizeof(ip))) {
        cJSON_Delete(j);
        return false;
    }
    uint32_t retry =
        s->phase == WIFI_LINK_BACKOFF && now < s->deadline_ms ? (uint32_t)(s->deadline_ms - now) : 0;
    bool ok = cJSON_AddStringToObject(j, "mode", "station") != NULL &&
              cJSON_AddStringToObject(j, "country", "US") != NULL &&
              cJSON_AddStringToObject(j, "state", wifi_link_phase_name(s->phase)) != NULL &&
              cJSON_AddBoolToObject(j, "associated", online || s->phase == WIFI_LINK_DHCP) != NULL &&
              cJSON_AddBoolToObject(j, "connected", online) != NULL &&
              (online ? cJSON_AddStringToObject(j, "ip", ip) : cJSON_AddNullToObject(j, "ip")) != NULL &&
              (s->rssi_valid ? cJSON_AddNumberToObject(j, "rssi", s->rssi)
                             : cJSON_AddNullToObject(j, "rssi")) != NULL &&
              cJSON_AddNumberToObject(j, "channel", s->channel) != NULL &&
              cJSON_AddNumberToObject(j, "attempts", s->attempts) != NULL &&
              cJSON_AddNumberToObject(j, "failure_streak", s->failures) != NULL &&
              cJSON_AddNumberToObject(j, "retry_in_ms", retry) != NULL &&
              (s->disconnect_reason ? cJSON_AddNumberToObject(j, "disconnect_reason", s->disconnect_reason)
                                    : cJSON_AddNullToObject(j, "disconnect_reason")) != NULL &&
              (s->last_error ? cJSON_AddNumberToObject(j, "last_error", s->last_error)
                             : cJSON_AddNullToObject(j, "last_error")) != NULL;
    ok = ok && cJSON_PrintPreallocated(j, out, (int)capacity, false);
    cJSON_Delete(j);
    if (!ok)
        out[0] = 0;
    return ok;
}
