#include "mcp2515.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

// MCP2515 registers
#define RXB0CTRL  0x60
#define RXB0SIDH  0x61
#define RXB0SIDL  0x62
#define RXB0DLC   0x65
#define RXB0D0    0x66
#define CANSTAT   0x0E
#define CANCTRL   0x0F
#define CNF3      0x28
#define CNF2      0x29
#define CNF1      0x2A
#define CANINTF   0x2C

// SPI commands
#define CMD_RESET  0xC0
#define CMD_READ   0x03
#define CMD_WRITE  0x02
#define CMD_RTS    0x80
#define CMD_READ_RX0 0x90
#define CMD_STATUS 0xA0

static spi_device_handle_t spi;
static bool initialized = false;

static uint8_t spi_read_reg(uint8_t addr) {
    uint8_t tx[3] = {CMD_READ, addr, 0x00};
    uint8_t rx[3] = {0};
    spi_transaction_t t = {
        .length = 24,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_transmit(spi, &t);
    return rx[2];
}

static void spi_write_reg(uint8_t addr, uint8_t val) {
    uint8_t tx[3] = {CMD_WRITE, addr, val};
    spi_transaction_t t = {
        .length = 24,
        .tx_buffer = tx,
    };
    spi_device_transmit(spi, &t);
}

static void spi_reset(void) {
    uint8_t tx[1] = {CMD_RESET};
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = tx,
    };
    spi_device_transmit(spi, &t);
}

static uint8_t spi_status(void) {
    uint8_t tx[2] = {CMD_STATUS, 0x00};
    uint8_t rx[2] = {0};
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_transmit(spi, &t);
    return rx[1];
}

bool mcp2515_init(spi_host_device_t host, int cs_pin) {
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 10000000, // 10 MHz
        .mode = 0,
        .spics_io_num = cs_pin,
        .queue_size = 1,
    };

    esp_err_t ret = spi_bus_initialize(host, &bus_cfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK) {
        printf("MCP: SPI bus init failed: %d\n", ret);
        return false;
    }
    ret = spi_bus_add_device(host, &dev_cfg, &spi);
    if (ret != ESP_OK) {
        printf("MCP: SPI device add failed: %d\n", ret);
        return false;
    }

    // Reset and check
    spi_reset();
    vTaskDelay(pdMS_TO_TICKS(10));

    // Verify communication - CANSTAT should be 0x80 after reset
    uint8_t canstat = spi_read_reg(CANSTAT);
    printf("MCP: CANSTAT=0x%02X (expect 0x80)\n", canstat);

    if (canstat == 0x00 || canstat == 0xFF) {
        printf("MCP: Device not responding on SPI. Check wiring.\n");
        return false;
    }

    // Configure for 500kbps @ 16MHz
    // Tested values: CNF1=0x00 CNF2=0x90 CNF3=0x02
    spi_write_reg(CNF1, 0x00);
    spi_write_reg(CNF2, 0x90);
    spi_write_reg(CNF3, 0x02);

    // RX buffer 0: accept all messages, no filters
    spi_write_reg(RXB0CTRL, 0x60); // Receive all, rollover disabled

    // Set listen-only mode (silent, no ACK, no TX)
    spi_write_reg(CANCTRL, 0x60);

    // Verify mode
    vTaskDelay(pdMS_TO_TICKS(10));
    canstat = spi_read_reg(CANSTAT);
    printf("MCP: CANSTAT=0x%02X (mode=%d)\n", canstat, (canstat >> 5) & 0x07);

    uint8_t cnf1 = spi_read_reg(CNF1);
    uint8_t cnf2 = spi_read_reg(CNF2);
    uint8_t cnf3 = spi_read_reg(CNF3);
    printf("MCP: CNF1=0x%02X CNF2=0x%02X CNF3=0x%02X\n", cnf1, cnf2, cnf3);

    if (cnf1 == 0x00 && cnf2 == 0x90 && cnf3 == 0x02) {
        printf("MCP: Init OK - 500kbps listen-only\n");
        initialized = true;
        return true;
    }

    printf("MCP: Warning - config registers didn't stick\n");
    initialized = true;
    return true;
}

bool mcp2515_read(can_frame_t *frame) {
    if (!initialized) return false;

    // Check RX0IF flag
    uint8_t intf = spi_read_reg(CANINTF);
    if (!(intf & 0x01)) return false;

    // Read RX buffer 0 via fast read command
    uint8_t tx[14] = {CMD_READ_RX0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t rx[14] = {0};
    spi_transaction_t t = {
        .length = 14 * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_transmit(spi, &t);

    frame->id = ((uint32_t)(rx[1] & 0x07) << 8) | rx[2];
    frame->dlc = rx[5] & 0x0F;
    if (frame->dlc > 8) frame->dlc = 8;
    memcpy(frame->data, &rx[6], frame->dlc);

    // Clear interrupt
    spi_write_reg(CANINTF, 0x00);
    return true;
}

bool mcp2515_self_test(void) {
    if (!initialized) return false;
    uint8_t canstat = spi_read_reg(CANSTAT);
    return (canstat != 0x00 && canstat != 0xFF);
}
