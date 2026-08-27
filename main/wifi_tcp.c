#include "wifi_tcp.h"
#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define WIFI_SSID "KTM-CAN"
#define WIFI_PASS "12345678"
#define TCP_PORT  8888

static int client_fd = -1;
static int server_fd = -1;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        printf("WiFi: client connected\n");
    }
}

static void tcp_server_task(void *pv) {
    struct sockaddr_in addr;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { printf("TCP: socket failed\n"); vTaskDelete(NULL); return; }

    int opt = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { printf("TCP: bind failed\n"); vTaskDelete(NULL); return; }
    listen(server_fd, 1);
    printf("WiFi: TCP server on port %d\n", TCP_PORT);

    while (1) {
        struct sockaddr_in c_addr; socklen_t len = sizeof(c_addr);
        client_fd = accept(server_fd, (struct sockaddr*)&c_addr, &len);
        if (client_fd >= 0) printf("WiFi: TCP client connected\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void wifi_tcp_init(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);

    wifi_config_t ap = { .ap = { .ssid = WIFI_SSID, .ssid_len = strlen(WIFI_SSID),
        .password = WIFI_PASS, .max_connection = 1, .authmode = WIFI_AUTH_WPA2_PSK } };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    esp_wifi_start();
    printf("WiFi: AP '%s' starting...\n", WIFI_SSID);

    xTaskCreate(tcp_server_task, "tcp", 4096, NULL, 5, NULL);
}

bool wifi_has_client(void) {
    return client_fd >= 0;
}

void wifi_tcp_send(const uint8_t *data, int len) {
    if (client_fd < 0) return;
    int sent = send(client_fd, data, len, 0);
    if (sent < 0) { close(client_fd); client_fd = -1; }
}
