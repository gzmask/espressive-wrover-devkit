#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WIFI_LINK_CONFIG_MS = 120000,
    WIFI_LINK_CONNECT_MS = 25000,
    WIFI_LINK_DHCP_MS = 30000,
    WIFI_LINK_STABLE_MS = 60000,
    WIFI_LINK_MAX_FAILURES = 7
};
typedef enum {
    WIFI_LINK_OFF,
    WIFI_LINK_CONFIG,
    WIFI_LINK_CONNECTING,
    WIFI_LINK_DHCP,
    WIFI_LINK_ONLINE,
    WIFI_LINK_BACKOFF,
    WIFI_LINK_ERROR
} wifi_link_phase_t;
typedef struct {
    wifi_link_phase_t phase;
    uint32_t attempts;
    uint32_t failures;
    uint16_t disconnect_reason;
    int32_t last_error;
    uint64_t since_ms;
    uint64_t deadline_ms;
    uint8_t ipv4[4];
    uint8_t channel;
    int8_t rssi;
    bool rssi_valid;
} wifi_link_snapshot_t;

wifi_link_snapshot_t wifi_link_initial(void);
const char *wifi_link_phase_name(wifi_link_phase_t phase);
bool wifi_link_valid(const wifi_link_snapshot_t *s);
bool wifi_link_configure(wifi_link_snapshot_t *s, uint64_t now);
bool wifi_link_begin(wifi_link_snapshot_t *s, uint64_t now);
bool wifi_link_associated(wifi_link_snapshot_t *s, uint8_t channel, uint64_t now);
bool wifi_link_ipv4(wifi_link_snapshot_t *s, const uint8_t ip[4], uint64_t now);
bool wifi_link_renew_ipv4(wifi_link_snapshot_t *s, const uint8_t ip[4]);
bool wifi_link_failed(wifi_link_snapshot_t *s, uint16_t reason, int32_t error, uint64_t now);
bool wifi_link_fatal(wifi_link_snapshot_t *s, int32_t error, uint64_t now);
bool wifi_link_signal(wifi_link_snapshot_t *s, int8_t rssi);
bool wifi_link_stable(wifi_link_snapshot_t *s, uint64_t now);
bool wifi_link_due(const wifi_link_snapshot_t *s, uint64_t now);
uint32_t wifi_link_backoff_ms(uint32_t failures);
bool wifi_link_format_ip(const wifi_link_snapshot_t *s, char *out, size_t capacity);
bool wifi_link_format_json(const wifi_link_snapshot_t *s, uint64_t now, char *out, size_t capacity);
