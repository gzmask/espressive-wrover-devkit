#include "repl_tcp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lisp_repl.h"
#include "lwip/sockets.h"
#include "repl_tcp_session.h"
#include "wifi_service.h"
#include <errno.h>
#include <stdatomic.h>
#include <unistd.h>

static atomic_bool listening;
static const char *TAG = "repl_tcp";
static repl_remote_status_t evaluate(const char *source, size_t length, char *reply, size_t capacity)
{
    esp_err_t error = lisp_repl_remote_eval(source, length, reply, capacity);
    return error == ESP_OK                  ? REPL_REMOTE_OK
           : error == ESP_ERR_INVALID_STATE ? REPL_REMOTE_BUSY
           : error == ESP_ERR_TIMEOUT       ? REPL_REMOTE_TIMEOUT
                                            : REPL_REMOTE_FAILED;
}
static void tcp_task(void *argument)
{
    (void)argument;
    /* Lisp is started before Wi-Fi/netif. Never call sockets before lwIP init. */
    wifi_link_snapshot_t link;
    while (!wifi_service_get_link_snapshot(&link) || link.phase != WIFI_LINK_ONLINE)
        vTaskDelay(pdMS_TO_TICKS(100));
    int server = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    struct sockaddr_in address = {
        .sin_family = AF_INET, .sin_port = htons(2323), .sin_addr.s_addr = htonl(INADDR_ANY)};
    if (server < 0)
        goto failed;
    if (bind(server, (struct sockaddr *)&address, sizeof(address)) != 0 || listen(server, 1) != 0)
        goto failed;
    atomic_store(&listening, true);
    ESP_LOGW(TAG, "plain TCP REPL ready; port=2323; NO TLS/token; trusted LAN only");
    for (;;) {
        int client = accept(server, NULL, NULL);
        if (client < 0 && errno == EINTR)
            continue;
        if (client < 0)
            break;
        /* One client, one RAM command at a time. No per-client tasks/history. */
        struct timeval timeout = {.tv_sec = 5, .tv_usec = 0};
        if (setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0)
            repl_tcp_session(client, evaluate);
        close(client);
    }
failed:
    atomic_store(&listening, false);
    if (server >= 0)
        close(server);
    ESP_LOGE(TAG, "TCP REPL stopped; no automatic restart");
    vTaskDelete(NULL);
}
esp_err_t repl_tcp_start(void)
{
    return xTaskCreate(tcp_task, "repl_tcp", 8192, NULL, 1, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
bool repl_tcp_is_healthy(void)
{
    return atomic_load(&listening);
}
