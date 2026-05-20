/**
 * @file wifi_service.c
 * @brief WiFi 服务层 (Modbus 专用)
 */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_service.h"
#include "mcu_api.h"
#include "system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "network_service.h"

static const char *TAG = "wifi_service";

uint8_t ccu_device_id[7] = {0};

void wifi_recv_buffer(uint8_t *data, uint32_t length)
{
    while (length--)
    {
        wifi_uart_receive_input(*data++);
    }
}

static void wifi_service_task(void *param)
{
    while (1)
    {
        wifi_uart_service();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void ccu_device_id_convert(void)
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);

    ccu_device_id[0] = DEVICE_TYPE_MODBUS;
    ccu_device_id[1] = mac[5];
    ccu_device_id[2] = mac[4];
    ccu_device_id[3] = mac[3];
    ccu_device_id[4] = mac[2];
    ccu_device_id[5] = mac[1];
    ccu_device_id[6] = mac[0];

    ESP_LOGI(TAG, "CCU [%02X] ID %02X%02X%02X%02X%02X%02X",
             ccu_device_id[0], ccu_device_id[1], ccu_device_id[2],
             ccu_device_id[3], ccu_device_id[4], ccu_device_id[5], ccu_device_id[6]);
}

void wifi_service_init(void)
{
    network_service_init();
    ccu_device_id_convert();
    wifi_service_queue_init();
    xTaskCreatePinnedToCore(wifi_service_task, "wifi-svc", 4096, NULL, 3, NULL, tskNO_AFFINITY);
}
