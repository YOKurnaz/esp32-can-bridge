# ESP32 CAN Bridge — KTM 1290 SAS

ESP-IDF firmware for an **ESP32 + MCP2515** CAN interface that reads the
KTM 1290 Super Adventure SAS CAN bus (engine ECU, `0x7EA`) and exposes the
frames to the Android dashboard/loggers over three transports:

| Transport | File | Notes |
|---|---|---|
| **BLE** | `ble_server.c` / `ble2.c` | GATT server (used by `Esp32BleConnection`) |
| **Bluetooth SPP** | `bt_spp.c` | RFCOMM serial (used by `ktm_bt_logger`) |
| **WiFi AP + TCP** | `wifi_tcp.c` | Access point with a TCP server |

- **MCP2515** SPI driver: `mcp2515.c` (CAN controller on SPI)
- Entry point: `main.c` — boots all transports
- Config: `sdkconfig.defaults` (build config; `sdkconfig` is generated locally)

## Build & flash

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # ESP-IDF v5.x
```

## ⚠️ WiFi AP defaults

The WiFi AP ships with `KTM-CAN` / `12345678` (`main/wifi_tcp.c`). Change
`WIFI_SSID` / `WIFI_PASS` (or move them to sdkconfig) before any deployment
outside the bench.

## Related repos
- `ktm_dashboard` — Android dashboard app (BLE transport)
- `ktm_usb_logger` / `ktm_bt_logger` — Android loggers (USB serial / BT SPP)
