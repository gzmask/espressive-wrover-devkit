#include "repl_boot_info.h"
#include <stdio.h>

bool repl_boot_banner(char *out, size_t capacity, const char *version, bool wifi)
{
    if (out == NULL || capacity == 0)
        return false;
    out[0] = 0;
    if (version == NULL)
        return false;
    int length = snprintf(out, capacity, "ESP-WROVER %s\nUSB REPL: 115200\n%s:help for commands\n", version,
                          wifi ? "Wi-Fi: connecting...\n" : "");
    return length >= 0 && (size_t)length < capacity;
}
bool repl_network_banner(char *out, size_t capacity, const uint8_t ipv4[4], bool ota)
{
    if (out == NULL || capacity == 0)
        return false;
    out[0] = 0;
    if (ipv4 == NULL)
        return false;
    int length = snprintf(out, capacity,
                          "\nIP: %u.%u.%u.%u\nREPL: TCP2323 (no login)\n"
                          "nc %u.%u.%u.%u 2323\n%s",
                          (unsigned)ipv4[0], (unsigned)ipv4[1], (unsigned)ipv4[2], (unsigned)ipv4[3],
                          (unsigned)ipv4[0], (unsigned)ipv4[1], (unsigned)ipv4[2], (unsigned)ipv4[3],
                          ota ? "OTA: HTTPS8443\n" : "");
    return length >= 0 && (size_t)length < capacity;
}
