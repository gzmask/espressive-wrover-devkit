#include "repl_boot_info.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void startup(void)
{
    char text[REPL_BOOT_INFO_CAPACITY];
    assert(repl_boot_banner(text, sizeof(text), "0.2.4-boot-info-ota", true, false));
    assert(strstr(text, "0.2.4-boot-info-ota"));
    assert(strstr(text, "USB REPL: 115200"));
    assert(strstr(text, "Wi-Fi: connecting..."));
    assert(strstr(text, ":help"));
    assert(!strstr(text, "eval") && !strstr(text, "(+ 1 2)"));
}
static void network(void)
{
    char text[REPL_BOOT_INFO_CAPACITY];
    const uint8_t ip[4] = {192, 0, 2, 1};
    assert(repl_network_banner(text, sizeof(text), ip, true));
    assert(strstr(text, "IP: 192.0.2.1"));
    assert(strstr(text, "REPL: TCP2323 (no login)"));
    assert(strstr(text, "nc 192.0.2.1 2323"));
    assert(strstr(text, "OTA: HTTPS8443"));
    assert(!strstr(text, "token") && !strstr(text, "Bearer"));
}
static void minimal(void)
{
    char text[REPL_BOOT_INFO_CAPACITY];
    const uint8_t ip[4] = {255, 255, 255, 255};
    assert(repl_boot_banner(text, sizeof(text), "dev", false, false));
    assert(!strstr(text, "Wi-Fi"));
    assert(repl_network_banner(text, sizeof(text), ip, false));
    assert(strstr(text, "nc 255.255.255.255 2323"));
    assert(!strstr(text, "OTA"));
}
static void usb(void)
{
    char text[REPL_BOOT_INFO_CAPACITY];
    assert(repl_boot_banner(text, sizeof(text), "0.3.0-single-usb", true, true));
    assert(!strcmp(text, "ESP-WROVER 0.3.0-single-usb\nUSB REPL: 115200\n"
                         "Firmware: USB only\nWi-Fi: connecting...\n:help for commands\n"));
    assert(!strstr(text, "OTA") && !strstr(text, "eval"));
}
static void bounds(void)
{
    const uint8_t ip[4] = {192, 0, 2, 1};
    char tiny[2] = {0};
    assert(!repl_boot_banner(tiny, sizeof(tiny), "dev", true, false) && tiny[1] == 0);
    assert(!repl_network_banner(tiny, sizeof(tiny), ip, true) && tiny[1] == 0);
    assert(!repl_boot_banner(NULL, 20, "dev", true, false));
    assert(!repl_boot_banner(tiny, 0, "dev", false, false));
    assert(!repl_boot_banner(tiny, sizeof(tiny), NULL, false, false));
    assert(!repl_network_banner(tiny, sizeof(tiny), NULL, false));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "startup"))
        startup();
    else if (!strcmp(argv[1], "network"))
        network();
    else if (!strcmp(argv[1], "minimal"))
        minimal();
    else if (!strcmp(argv[1], "usb"))
        usb();
    else if (!strcmp(argv[1], "bounds"))
        bounds();
    else
        return 1;
    puts("PASS");
    return 0;
}
