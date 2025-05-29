/*
 * Copyrear (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-08-04     Rick       the first version
 */
#include "wifi_api.h"
#include "network_typedef.h"
#include "crypto_aes.h"
#include "esp_timer.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "heater_noritz_api.h"
#include "heater_rinnai_api.h"
#include "heater_rinnai_bussiness_api.h"
#include "heater_interface_api.h"
#include "dishwasher_modbus_api.h"
#include "esp_log.h"

esp_timer_handle_t heater_heart_timer;
esp_timer_handle_t heater_poll_upload_timer;
esp_timer_handle_t heater_poll_timer;
esp_timer_handle_t heater_detect_timer;

uint8_t tcp_send_count = 0x01;
uint8_t heater_detect_done = 0;

extern uint8_t smartconfig_start_flag;

uint8_t tcp_send_count_read(void)
{
    return tcp_send_count++;
}

void wifi_heater_common_key_request(void)
{
    uint8_t plain_buf[8] = {0};
    uint8_t *encrypt_ptr;

    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    plain_buf[0] = 0;
    plain_buf[1] = 1;

    crypto_aes_local_encrypt(plain_buf,2,&encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xff, 2, send_len);

    free(encrypt_ptr);
}

void wifi_heater_common_heart_upload(void)
{
    uint8_t plain_buf[8] = {0};
    uint8_t *encrypt_ptr;

    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    plain_buf[0] = tcp_send_count_read();
    plain_buf[1] = 1;

    crypto_aes_remote_encrypt(plain_buf,2,&encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xff, 2, send_len);

    free(encrypt_ptr);
}

static void heater_poll_upload_timer_callback(void* arg)
{
    heater_interface_status_reset();
}

static void heater_heart_timer_callback(void* arg)
{
    if(heater_detect_done == 1 && smartconfig_start_flag == 0)
    {
        wifi_heater_common_heart_upload();
        esp_timer_start_once(heater_poll_upload_timer, 15 * 1000 * 1000);
    }
}

void heater_heart_timer_start(void)
{
    esp_timer_stop(heater_heart_timer);
    esp_timer_start_periodic(heater_heart_timer, 30 * 1000 * 1000);
}

void heater_heart_timer_stop(void)
{
    esp_timer_stop(heater_heart_timer);
}

void heater_heart_timer_init(void)
{
    const esp_timer_create_args_t heart_timer_args = 
    {
        .callback = &heater_heart_timer_callback,
        .name = "heater_heart_timer"
    };

    const esp_timer_create_args_t heater_poll_upload_timer_args = 
    {
        .callback = &heater_poll_upload_timer_callback,
        .name = "heater_poll_upload_timer"
    };
    
    ESP_ERROR_CHECK(esp_timer_create(&heart_timer_args, &heater_heart_timer));
    ESP_ERROR_CHECK(esp_timer_create(&heater_poll_upload_timer_args, &heater_poll_upload_timer));
}

static void heater_poll_timer_callback(void* arg)
{
    if(heater_detect_done == 1 && smartconfig_start_flag == 0)
    {
        // if(hearter_device_type_get() == HEATER_TYPE_RINNAL_BUSINESS)
        // {
        //     heater_rinnai_bussiness_poll_callback();
        // }
        // else
        // {
        //     heater_interface_error_read();
        // }
        dish_washer_modbus_poll();
    }
}

void heater_poll_timer_start(void)
{
    esp_timer_stop(heater_poll_timer);
    // if(hearter_device_type_get() == HEATER_TYPE_RINNAL_BUSINESS)
    // {
    //     esp_timer_start_periodic(heater_poll_timer, 1500 * 1000);
    // }
    // else
    // {
    //     esp_timer_start_periodic(heater_poll_timer, 5 * 1000 * 1000);
    // }
    esp_timer_start_periodic(heater_poll_timer, 2000 * 1000);
}

void heater_poll_timer_init(void)
{
    const esp_timer_create_args_t timer_args = 
    {
        .callback = &heater_poll_timer_callback,
        .name = "heater_poll_timer"
    };
    
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &heater_poll_timer));
}

void heater_detect_finish(uint8_t value)
{
    // if(heater_detect_done == 0 && smartconfig_start_flag == 0)
    // {
    //     hearter_device_type_set(value);
    //     heater_detect_done = 1;
    //     esp_timer_stop(heater_detect_timer);
    //     heater_poll_timer_start();
    //     heater_heart_timer_start();
    // }
}

static void heater_detect_timer_callback(void* arg)
{
    // static uint8_t heater_detect_try = 0;
    // switch(heater_detect_try)
    // {
    //     case 0:
    //         heater_detect_try = 1;
    //         heater_noritz_info_read();
    //         break;
    //     case 1:
    //         heater_detect_try = 2;
    //         heater_rinnai_info_read();
    //         break;
    //     case 2:
    //         heater_detect_try = 0;
    //         heater_rinnai_bussiness_info_read();
    //         break;
    //     default:
    //         break;
    // }
    if(dish_washer_modbus_poll() == ESP_OK)
    {
        if(heater_detect_done == 0 && smartconfig_start_flag == 0)
        {
            heater_detect_done = 1;
            esp_timer_stop(heater_detect_timer);
            heater_poll_timer_start();
            heater_heart_timer_start();
        }
    }
}

void heater_detect_timer_init(void)
{
    const esp_timer_create_args_t timer_args = 
    {
        .callback = &heater_detect_timer_callback,
        .name = "heater_detect_timer"
    };
    
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &heater_detect_timer));
    esp_timer_start_periodic(heater_detect_timer, 2000 * 1000);
}