/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-12-21     Rick       the first version
 */
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "wifi_service.h"
#include "tcp_client.h"
#include "mcu_api.h"
#include "system.h"
#include "esp_wifi.h"

uint8_t ccu_device_id[7] = {0};

void wifi_recv_buffer(uint8_t *data,uint32_t length)
{
    while(length--)
    {
        wifi_uart_receive_input(*data++);
    }
}

void wifi_service_callback(void *parameter)
{
    while(1)
    {
        wifi_uart_service();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void ccu_device_id_convert(void)
{
    uint8_t macAddr[6];
    esp_wifi_get_mac(WIFI_IF_STA,macAddr);
    ccu_device_id[0] = DEVICE_TYPE_WATER_HEATER;
    ccu_device_id[1] = macAddr[5];
    ccu_device_id[2] = macAddr[4];
    ccu_device_id[3] = macAddr[3];
    ccu_device_id[4] = macAddr[2];
    ccu_device_id[5] = macAddr[1];
    ccu_device_id[6] = macAddr[0];
}

void wifi_service_init(void)
{
    tcp_client_init();
    ccu_device_id_convert();
    wifi_service_queue_init();
    xTaskCreatePinnedToCore(wifi_service_callback, "wifi-service", 4096, NULL, 3, NULL, tskNO_AFFINITY);
}