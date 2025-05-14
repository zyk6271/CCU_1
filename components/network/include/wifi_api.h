/*
 * Copyrear (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-08-04     Rick       the first version
 */
#ifndef WIFI_WIFI_API_H_
#define WIFI_WIFI_API_H_

#include "stdint.h"

uint8_t tcp_send_count_read(void);
void wifi_heater_common_key_request(void);
void wifi_heater_common_heart_upload(void);
void heater_heart_timer_start(void);
void heater_heart_timer_init(void);
void heater_poll_timer_init(void);
void heater_poll_timer_start(void);
void heater_detect_finish(uint8_t value);
void heater_detect_timer_init(void);

#endif /* WIFI_WIFI_API_H_ */
