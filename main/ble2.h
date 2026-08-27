#pragma once
#include <stdint.h>
#include <stdbool.h>
void ble2_init(void);
bool ble2_connected(void);
void ble2_send(const uint8_t *data, int len);
