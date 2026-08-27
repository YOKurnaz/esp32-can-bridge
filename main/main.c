#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "nvs_flash.h"
#include "mcp2515.h"
#include "bt_spp.h"

#define MCP_CS_PIN  GPIO_NUM_5
#define LED_PIN     GPIO_NUM_2

void app_main(void)
{
    nvs_flash_init();

    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 1);

    printf("\n=== KTM CAN BRIDGE (MCP2515 + BT) ===\n");
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("Chip: ESP32 rev %d\n", chip_info.revision);

    printf("--- MCP2515 INIT ---\n");
    if (!mcp2515_init(SPI2_HOST, MCP_CS_PIN)) {
        printf("FATAL: MCP2515 init failed\n");
        while (1) {
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

    // BT SPP section removed because we use BT BT SPP
    bt_spp_init();

    printf("\n=== READY ===\n");
    printf("BT: 'KTM CAN Bridge' BT SPP ready advertising\n");
    printf("Waiting for BT BT SPP client + CAN data...\n");

    int frame_count = 0;
    int seconds = 0;
    can_frame_t frame;

    while (1) {
        if (mcp2515_read(&frame)) {
            frame_count++;
            printf("CAN: ID=0x%03lX DLC=%d DATA=", frame.id, frame.dlc);
            for (int i = 0; i < frame.dlc; i++) printf("%02X ", frame.data[i]);
            printf("(%d)\n", frame_count);

            // Forward via BT SPP
                        char buf[128];
            int pos = snprintf(buf, sizeof(buf), "ID=0x%03lX,%d", frame.id, frame.dlc);
            for (int i = 0; i < frame.dlc && pos < (int)sizeof(buf) - 4; i++)
                pos += snprintf(buf + pos, sizeof(buf) - pos, ",%02X", frame.data[i]);
            buf[pos++] = '\n';
            bt_spp_send((uint8_t*)buf, pos);

            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(5));
            gpio_set_level(LED_PIN, 1);
        }
        vTaskDelay(pdMS_TO_TICKS(10));

        static int counter = 0;
        if (++counter >= 1000) {
            counter = 0; seconds += 10;
            printf("Status: %ds, frames=%d, MCP=%d, BT SPP=%d\n",
                seconds, frame_count, mcp2515_self_test(), bt_spp_is_connected());
        }
    }
}
