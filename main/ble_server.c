#include "ble_server.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "freertos/FreeRTOS.h"

#define SERVICE_UUID      0x00FF
#define CHAR_CAN_RAW_UUID 0xFF01

static uint16_t gatts_if = 0;
static uint16_t conn_id = 0;
static uint16_t can_handle = 0;
static bool connected = false;

static void gap_handler(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p);
static void gatts_handler(esp_gatts_cb_event_t e, esp_gatt_if_t iface, esp_ble_gatts_cb_param_t *p);

void ble_init(void) {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_ble_gap_register_callback(gap_handler);
    esp_ble_gatts_register_callback(gatts_handler);
    esp_ble_gap_set_device_name("KTM CAN Bridge");

    esp_ble_gatts_app_register(0);
    printf("BLE: Init done, advertising as 'KTM CAN Bridge'\n");
}

bool ble_is_connected(void) {
    return connected;
}

void ble_send_can_frame(uint32_t id, uint8_t dlc, const uint8_t *data) {
    if (!connected || can_handle == 0) return;
    char buf[128];
    int pos = snprintf(buf, sizeof(buf), "ID=0x%03lX,%d", id, dlc);
    for (int i = 0; i < dlc && pos < (int)sizeof(buf) - 4; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, ",%02X", data[i]);
    }
    esp_ble_gatts_send_indicate(gatts_if, conn_id, can_handle,
        strlen(buf), (uint8_t*)buf, false);
}

static void gap_handler(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p) {
    switch (e) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: {
        esp_ble_adv_params_t adv = {
            .adv_int_min = 0x20, .adv_int_max = 0x40,
            .adv_type = ADV_TYPE_IND,
            .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
            .channel_map = ADV_CHNL_ALL,
            .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        };
        esp_ble_gap_start_advertising(&adv);
        break;
    }
    default: break;
    }
}

static void gatts_handler(esp_gatts_cb_event_t event, esp_gatt_if_t iface, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        gatts_if = iface;
        printf("BLE: Registered (if=%d)\n", iface);
        {
            esp_gatt_srvc_id_t s;
            s.is_primary = true;
            s.id.inst_id = 0;
            s.id.uuid.len = ESP_UUID_LEN_16;
            s.id.uuid.uuid.uuid16 = SERVICE_UUID;
            esp_ble_gatts_create_service(iface, &s, 10);
        }
        break;

    case ESP_GATTS_CREATE_EVT:
        if (param->create.status == ESP_GATT_OK) {
            printf("BLE: Service created\n");
            esp_bt_uuid_t u = {.len = 2, .uuid = {.uuid16 = CHAR_CAN_RAW_UUID}};
            esp_ble_gatts_add_char(param->create.service_handle, &u,
                ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_NOTIFY, NULL, NULL);
            // Add CCCD descriptor
            esp_bt_uuid_t desc = {.len = ESP_UUID_LEN_16, .uuid = {.uuid16 = 0x2902}};
            esp_ble_gatts_add_char_descr(param->create.service_handle, &desc,
                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
            // CRITICAL: start the service after adding characteristics
            esp_ble_gatts_start_service(param->create.service_handle);
        }
        break;

    case ESP_GATTS_ADD_CHAR_EVT:
        can_handle = param->add_char.attr_handle + 1;
        printf("BLE: Char added (handle=0x%04X)\n", can_handle);
        break;

    case ESP_GATTS_START_EVT:
        printf("BLE: Service started, advertising...\n");
        {
            esp_ble_adv_data_t d = {
                .set_scan_rsp = true,
                .include_name = true,
                .include_txpower = false,
                .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
            };
            esp_ble_gap_config_adv_data(&d);
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        conn_id = param->connect.conn_id;
        connected = true;
        printf("BLE: Connected\n");
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        connected = false;
        conn_id = 0;
        can_handle = 0;
        printf("BLE: Disconnected, re-advertising...\n");
        {
            esp_ble_adv_params_t adv2 = {
                .adv_int_min = 0x20, .adv_int_max = 0x40,
                .adv_type = ADV_TYPE_IND,
                .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
                .channel_map = ADV_CHNL_ALL,
                .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
            };
            esp_ble_gap_start_advertising(&adv2);
        }
        break;

    default: break;
    }
}
