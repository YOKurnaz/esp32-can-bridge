#pragma once
#include <stdint.h>
#include <stdbool.h>

// Initialize BLE server, advertise as "KTM CAN Bridge"
void ble_init(void);

// Send a CAN frame via BLE notification (non-blocking)
void ble_send_can_frame(uint32_t id, uint8_t dlc, const uint8_t *data);

// Check if a BLE client is connected
bool ble_is_connected(void);
