#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "BLE2";

#define CHAR_UUID 0xFF01

static uint16_t gatts_if = 0, conn_id = 0, char_handle = 0;
static bool connected = false;

static void gap_cb(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p) {
    if (e == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT) {
        esp_ble_adv_params_t ap = {.adv_int_min=0x20, .adv_int_max=0x40, .adv_type=ADV_TYPE_IND,
            .own_addr_type=BLE_ADDR_TYPE_PUBLIC, .channel_map=ADV_CHNL_ALL,
            .adv_filter_policy=ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY};
        esp_ble_gap_start_advertising(&ap);
    }
}

static void gatts_cb(esp_gatts_cb_event_t event, esp_gatt_if_t gif, esp_ble_gatts_cb_param_t *p) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        gatts_if = gif;
        esp_gatt_srvc_id_t s = {.is_primary=true, .id={.inst_id=0, .uuid={.len=ESP_UUID_LEN_16, .uuid={.uuid16=0x00FF}}}};
        esp_ble_gatts_create_service(gif, &s, 6);
        break;
    case ESP_GATTS_CREATE_EVT:
        if (p->create.status == ESP_GATT_OK) {
            esp_bt_uuid_t u = {.len=2, .uuid={.uuid16=CHAR_UUID}};
            esp_ble_gatts_add_char(p->create.service_handle, &u, ESP_GATT_PERM_READ,
                ESP_GATT_CHAR_PROP_BIT_NOTIFY, NULL, NULL);
            esp_ble_gatts_start_service(p->create.service_handle);
        }
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
        char_handle = p->add_char.attr_handle + 1;
        printf("BLE: char handle=0x%04X\n", char_handle);
        break;
    case ESP_GATTS_START_EVT: {
        esp_ble_adv_data_t d = {.set_scan_rsp=false, .include_name=true, .include_txpower=false,
            .flag=(ESP_BLE_ADV_FLAG_GEN_DISC|ESP_BLE_ADV_FLAG_BREDR_NOT_SPT)};
        esp_ble_gap_config_adv_data(&d);
        printf("BLE: advertising as 'KTM CAN'\n");
        break;
    }
    case ESP_GATTS_CONNECT_EVT:
        connected = true; conn_id = p->connect.conn_id;
        printf("BLE: connected\n");
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        connected = false; conn_id = 0; char_handle = 0;
        printf("BLE: disconnected\n");
        esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){.adv_int_min=0x20,.adv_int_max=0x40,.adv_type=ADV_TYPE_IND,.own_addr_type=BLE_ADDR_TYPE_PUBLIC,.channel_map=ADV_CHNL_ALL,.adv_filter_policy=ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY});
        break;
    default: break;
    }
}

void ble2_init(void) {
    esp_bt_controller_config_t c = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&c);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_ble_gap_set_device_name("KTM CAN");
    esp_ble_gap_register_callback(gap_cb);
    esp_ble_gatts_register_callback(gatts_cb);
    esp_ble_gatts_app_register(0);
}

bool ble2_connected(void) { return connected && char_handle > 0; }

void ble2_send(const uint8_t *data, int len) {
    if (!connected || char_handle == 0) return;
    // Send via notification — CCCD may not be written, but the BLE stack
    // will queue it; once CCCD is enabled, queued data may flow
    esp_ble_gatts_send_indicate(gatts_if, conn_id, char_handle, len, (uint8_t*)data, false);
}
