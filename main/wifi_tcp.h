#pragma once
#include <stdint.h>
#include <stdbool.h>
void wifi_tcp_init(void);
bool wifi_has_client(void);
void wifi_tcp_send(const uint8_t *data, int len);
