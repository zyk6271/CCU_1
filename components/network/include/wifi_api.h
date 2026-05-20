/**
 * @file wifi_api.h
 * @brief WiFi 通信接口 (Modbus 专用)
 */

#ifndef WIFI_API_H_
#define WIFI_API_H_

#include <stdint.h>

uint8_t tcp_send_count_read(void);
void modbus_poll_timer_init(void);
void modbus_poll_timer_start(void);

#endif
