#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

// CAN frame structure
typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
} can_frame_t;

// Init MCP2515 over SPI. Returns true on success.
bool mcp2515_init(spi_host_device_t host, int cs_pin);

// Read a CAN frame if available. Returns true if frame was read.
bool mcp2515_read(can_frame_t *frame);

// Diagnostic: check if MCP2515 responds on SPI
bool mcp2515_self_test(void);
