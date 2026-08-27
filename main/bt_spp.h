#pragma once
#include <stdint.h>
#include <stdbool.h>

// Start Bluetooth SPP server
void bt_spp_init(void);

// Send CAN frame over SPP
void bt_spp_send(const uint8_t *data, int len);

// Check if client is connected
bool bt_spp_is_connected(void);
