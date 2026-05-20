/**
 * @file wifi_api.c
 * @brief WiFi 通信接口 (Modbus 专用)
 */

#include "wifi_api.h"
#include "network_typedef.h"
#include "crypto_aes.h"
#include "esp_timer.h"
#include "ccu_modbus_api.h"
#include "esp_log.h"
#include <string.h>
#include "freertos/FreeRTOS.h"

static esp_timer_handle_t modbus_poll_timer;

extern uint8_t smartconfig_start_flag;

uint8_t tcp_send_count = 0x01;

uint8_t tcp_send_count_read(void)
{
    return tcp_send_count++;
}

/* Modbus 心跳/轮询上传 */
static void modbus_poll_timer_callback(void *arg)
{
    if (smartconfig_start_flag == 0)
    {
        wifi_ccu_modbus_poll_upload();
    }
}

void modbus_poll_timer_start(void)
{
    esp_timer_stop(modbus_poll_timer);
    esp_timer_start_periodic(modbus_poll_timer, 2000 * 1000);  /* 2秒 */
}

void modbus_poll_timer_init(void)
{
    const esp_timer_create_args_t timer_args = {
        .callback = &modbus_poll_timer_callback,
        .name = "modbus_poll_timer"
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &modbus_poll_timer));
    esp_timer_start_periodic(modbus_poll_timer, 2000 * 1000);
}
