#include "bt_spp.h"
#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"

static bool connected = false;
static uint32_t spp_handle = 0;

static void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    switch (event) {
    case ESP_SPP_INIT_EVT:
        printf("BT: SPP init OK\n");
        esp_spp_start_srv(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_SLAVE, 0, "KTM_CAN");
        break;
    case ESP_SPP_START_EVT:
        if (param->start.status == ESP_SPP_SUCCESS) {
            spp_handle = param->start.handle;
            printf("BT: SPP server started\n");
            esp_bt_dev_set_device_name("KTM CAN Bridge");
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            printf("BT: Discoverable as 'KTM CAN Bridge'\n");
        } else {
            printf("BT: SPP start FAILED: %d\n", param->start.status);
        }
        break;
    case ESP_SPP_OPEN_EVT:
        connected = true;
        printf("BT: Client connected\n");
        break;
    case ESP_SPP_CLOSE_EVT:
        connected = false;
        printf("BT: Client disconnected\n");
        break;
    default: break;
    }
}

void bt_spp_init(void) {
    nvs_flash_init();
    
    esp_err_t ret;
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    printf("BT: mem_release=%d\n", ret);
    
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    printf("BT: controller_init=%d\n", ret);
    
    ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    printf("BT: controller_enable=%d\n", ret);
    
    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    printf("BT: bluedroid_init=%d\n", ret);
    
    ret = esp_bluedroid_enable();
    printf("BT: bluedroid_enable=%d\n", ret);
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    esp_spp_register_callback(spp_callback);
    ret = esp_spp_init(ESP_SPP_MODE_CB);
    printf("BT: spp_init=%d\n", ret);
}

bool bt_spp_is_connected(void) { return connected; }

void bt_spp_send(const uint8_t *data, int len) {
    if (!connected) return;
    esp_spp_write(spp_handle, len, (uint8_t*)data);
}
