#include "wifi_provision.h"
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

static void valid(void)
{
    const char *inputs[] = {"demo-net\ntestpass", "demo-net\ntestpass\n", "demo-net\r\ntestpass\r\n"};
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        wifi_credentials_t out;
        CHECK(wifi_credentials_from_text((const uint8_t *)inputs[i], strlen(inputs[i]), &out));
        CHECK(out.ssid_length == 8 && out.password_length == 8);
        CHECK(memcmp(out.ssid, "demo-net", 8) == 0 && memcmp(out.password, "testpass", 8) == 0);
    }
    const char spaces[] = " demo \n pass word \n";
    wifi_credentials_t out;
    CHECK(wifi_credentials_from_text((const uint8_t *)spaces, sizeof(spaces) - 1, &out));
    CHECK(out.ssid_length == 6 && out.password_length == 11);
    CHECK(memcmp(out.password, " pass word ", 11) == 0);
}
static void rejected(const uint8_t *data, size_t length)
{
    wifi_credentials_t out;
    memset(&out, 0xaa, sizeof(out));
    CHECK(!wifi_credentials_from_text(data, length, &out));
    const uint8_t *bytes = (const uint8_t *)&out;
    for (size_t i = 0; i < sizeof(out); i++)
        CHECK(bytes[i] == 0);
}
static void malformed(void)
{
    const char *inputs[] = {"",
                            "demo-net",
                            "\ntestpass",
                            "demo-net\nshort",
                            "demo-net\ntestpass\n\n",
                            "demo-net\ntestpass\nextra",
                            "bad\tnet\ntestpass",
                            "demo-net\nbad\tpass"};
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++)
        rejected((const uint8_t *)inputs[i], strlen(inputs[i]));
    const uint8_t nul[] = {'s', '\n', 'p', 'a', 's', 's', 0, 'o', 'r', 'd'};
    rejected(nul, sizeof(nul));
    rejected(NULL, 12);
    uint8_t huge[257] = {0};
    rejected(huge, sizeof(huge));
    CHECK(!wifi_credentials_from_text((const uint8_t *)"s\ntestpass", 10, NULL));
}
static void bounds(void)
{
    uint8_t input[100];
    memset(input, 's', 33);
    input[32] = '\n';
    memset(input + 33, 'p', 63);
    wifi_credentials_t out;
    CHECK(wifi_credentials_from_text(input, 96, &out));
    CHECK(out.ssid_length == 32 && out.password_length == 63);
    memset(input + 33, 'a', 64);
    CHECK(wifi_credentials_from_text(input, 97, &out));
    CHECK(out.password_length == 64);
    input[96] = 'g';
    rejected(input, 97);
    input[32] = 's';
    input[33] = '\n';
    memset(input + 34, 'p', 8);
    rejected(input, 42);
}
int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    if (strcmp(argv[1], "valid") == 0)
        valid();
    else if (strcmp(argv[1], "malformed") == 0)
        malformed();
    else if (strcmp(argv[1], "bounds") == 0)
        bounds();
    else
        return 1;
    puts("PASS");
    return 0;
}
