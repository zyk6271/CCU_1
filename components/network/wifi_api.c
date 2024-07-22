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
#include "heater_rinnai_api.h"
#include "heater_noritz_api.h"
#include "esp_log.h"

esp_timer_handle_t heater_heart_timer;
esp_timer_handle_t heater_poll_upload_timer;
esp_timer_handle_t heater_poll_timer;
esp_timer_handle_t heater_detect_timer;

uint8_t device_type = 1;//0 is rinnai,1 is noritz
uint8_t heater_detect_done = 0;
uint8_t heater_detect_flag = 0;
uint32_t send_counter = 0x01;
extern uint8_t smartconfig_start_flag;

void wifi_poll_status_reset(void)
{
    if(device_type)
    {
        heater_noritz_poll_status_resset();
    }
    else
    {
        heater_rinnai_poll_status_resset();
    }
}

void wifi_recv_info_upload(void)
{
    if(device_type)
    {
        heater_noritz_error_read();
    }
    else
    {
        heater_rinnai_error_read();
    }
}

void wifi_recv_model_upload(void)
{
    if(device_type)
    {
        heater_noritz_info_read();
    }
    else
    {
        heater_rinnai_info_read();
    }
}

void wifi_recv_temperature_setting(uint8_t value)
{
    if(device_type)
    {
        heater_noritz_temperature_write(value);
    }
    else
    {
        heater_rinnai_temperature_write(value);
    }
}

void wifi_recv_eco_setting(uint8_t value)
{
    if(device_type)
    {
        heater_noritz_eco_write(value);
    }
    else
    {
        heater_rinnai_eco_write(value);
    }
}

void wifi_recv_circulation_setting(uint8_t value)
{
    if(device_type)
    {
        heater_noritz_circulation_write(value);
    }
    else
    {
        heater_rinnai_circulation_write(value);
    }
}

void wifi_recv_power_setting(uint8_t value)
{
    if(device_type)
    {
        heater_noritz_power_write(value);
    }
    else
    {
        heater_rinnai_power_write(value);
    }
}

void wifi_recv_priority_setting(uint8_t value)
{
    if(device_type)
    {
        heater_noritz_priority_write(value);
    }
    else
    {
        heater_rinnai_priority_write(value);
    }
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

    plain_buf[0] = send_counter++;
    plain_buf[1] = 1;

    crypto_aes_remote_encrypt(plain_buf,2,&encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xff, 2, send_len);

    free(encrypt_ptr);
}

static void heater_poll_upload_timer_callback(void* arg)
{
    wifi_poll_status_reset();
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
        wifi_recv_info_upload();
    }
}

void heater_poll_timer_start(void)
{
    esp_timer_stop(heater_poll_timer);
    esp_timer_start_periodic(heater_poll_timer, 5 * 1000 * 1000);
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
    if(heater_detect_done == 0 && smartconfig_start_flag == 0)
    {
        device_type = value;
        heater_detect_done = 1;
        esp_timer_stop(heater_detect_timer);
    }
}

static void heater_detect_timer_callback(void* arg)
{
    if(heater_detect_flag)
    {
        heater_detect_flag = 0;
        heater_rinnai_info_read();
    }
    else
    {
        heater_detect_flag = 1;
        heater_noritz_info_read();
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
    esp_timer_start_periodic(heater_detect_timer, 1 * 1000 * 1000);
}