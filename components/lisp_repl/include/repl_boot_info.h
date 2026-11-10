#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { REPL_BOOT_INFO_CAPACITY = 192 };
bool repl_boot_banner(char *out, size_t capacity, const char *version, bool wifi, bool usb_updates);
bool repl_network_banner(char *out, size_t capacity, const uint8_t ipv4[4], bool ota);
