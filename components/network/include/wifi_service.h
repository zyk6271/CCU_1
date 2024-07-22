/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-12-21     Rick       the first version
 */
#ifndef WIFI_SERVICE_H_
#define WIFI_SERVICE_H_

#include "stdint.h"

void wifi_service_init(void);
void wifi_recv_byte(uint8_t data);
void wifi_recv_buffer(uint8_t *data,uint32_t length);

#endif /* WIFI_WIFI_UART_H_ */
