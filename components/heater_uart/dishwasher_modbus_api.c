/*
 * SPDX-FileCopyrightText: 2016-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#include "string.h"
#include "esp_log.h"
#include "mbcontroller.h"
#include "sdkconfig.h"
#include "dishwasher_modbus_api.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "string.h"
#include "crypto_aes.h"
#include "network_typedef.h"
#include "wifi_api.h"
#include "wifi_manager.h"
#include "heater_interface_api.h"

static const char *TAG = "dishwasher_modbus";

#define MB_TXD_PIN (GPIO_NUM_21)
#define MB_RXD_PIN (GPIO_NUM_20)
#define MB_RE_PIN  (GPIO_NUM_10)

#define MB_PORT_NUM     (UART_NUM_0)   // Number of UART port used for Modbus connection
#define MB_DEV_SPEED    (9600)  // The communication speed of the UART

#define OPTS(min_val, max_val, step_val) { .opt1 = min_val, .opt2 = max_val, .opt3 = step_val }

// Enumeration of modbus device addresses accessed by master device
enum {
    MB_DEVICE_ADDR1 = 0x08 // Only one slave device used for the test (add other slave addresses here)
};

// Enumeration of all supported CIDs for device (used in parameter definition table)
enum {
    CID_TOTAL_HOLDING ,
    CID_COUNT
};

static void *modbus_handle = NULL;

// Access Mode - can be used to implement custom options for processing of characteristic (Read/Write restrictions, factory mode values and etc).
const mb_parameter_descriptor_t device_parameters[] = {
    {
        CID_TOTAL_HOLDING,
        "TOTAL_HOLDING",         // 名称
        "--",                    // 无单位
        MB_DEVICE_ADDR1,         // 从机地址（需与设备一致）
        MB_PARAM_HOLDING,        // 输入寄存器（只读）
        0x1320,                  // 寄存器起始地址
        44,                      // 占用44个寄存器（16位）
        0,                       // 数据偏移量（需根据实际结构体调整）
        PARAM_TYPE_U16_BA,       // 16位无符号整数
        88,                      // 2字节
        OPTS(0, 0, 0),           // 无范围限制
        PAR_PERMS_READ           // 只读
    },
};

static uint8_t modbus_read_value[512] = {0};
const uint16_t num_device_parameters = (sizeof(device_parameters)/sizeof(device_parameters[0]));

void dish_washer_info_upload(void)
{
    uint8_t plain_buf[41] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    // 按寄存器地址分段拷贝（单位：字节）
    plain_buf[0] = tcp_send_count_read();;
    memcpy(&plain_buf[1],  modbus_read_value + (0x1321 - 0x1320) * 2, 2);  // 0x1321（2字节）
    memcpy(&plain_buf[3],  modbus_read_value + (0x1324 - 0x1320) * 2, 8);  // 0x1324~0x1327（4寄存器 × 2 = 8字节）
    memcpy(&plain_buf[11], modbus_read_value + (0x132C - 0x1320) * 2, 22); // 0x132C~0x1336（11寄存器 × 2 = 22字节）
    memcpy(&plain_buf[33], modbus_read_value + (0x133B - 0x1320) * 2, 6);  // 0x133B~0x133D（3寄存器 × 2 = 6字节）
    memcpy(&plain_buf[39], modbus_read_value + (0x1349 - 0x1320) * 2, 2);  // 0x1349（2字节）

    ESP_LOG_BUFFER_HEXDUMP("dish_washer_info_upload", plain_buf, 41, ESP_LOG_INFO);

    crypto_aes_remote_encrypt(plain_buf, 41, &encrypt_ptr, &encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xB0, 41, send_len);

    free(encrypt_ptr);
}

void dishwasher_poll_status_reset(void)
{
    memset(modbus_read_value,0,sizeof(modbus_read_value));
}

esp_err_t dish_washer_modbus_poll(void)
{
    int cid = 0;
    uint8_t type = 0;  
    esp_err_t err = ESP_OK;
    uint8_t modbus_read_value_temp[512] = {0};             

    const mb_parameter_descriptor_t* param_descriptor = NULL;
    err = mbc_master_get_cid_info(modbus_handle, cid, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) 
    {
        err = mbc_master_get_parameter(modbus_handle, param_descriptor->cid, modbus_read_value_temp, &type);
        if (err == ESP_OK) 
        {
            ESP_LOGI(TAG, "modbus poll read success\r\n");
            //ESP_LOG_BUFFER_HEXDUMP("modbus_read_value_temp", modbus_read_value_temp, 88, ESP_LOG_INFO);
            if(memcmp(modbus_read_value_temp,modbus_read_value,88) == 0)
            {
                ESP_LOGI(TAG, "holding register value no change\r\n");
            }
            else
            {
                memcpy(modbus_read_value,modbus_read_value_temp,88);
                dish_washer_info_upload();
                ESP_LOGI(TAG, "holding register value has change\r\n");
            }
        } 
        else 
        {
            ESP_LOGE(TAG, "modbus poll read failed\r\n");
            return ESP_FAIL;
        }
    } 

    return ESP_OK; 
}
void dish_washer_modbus_init(void)
{
    // Initialize Modbus controller
    mb_communication_info_t comm = {
        .ser_opts.port = MB_PORT_NUM,
        .ser_opts.mode = MB_RTU,
        .ser_opts.baudrate = MB_DEV_SPEED,
        .ser_opts.parity = MB_PARITY_NONE,
        .ser_opts.uid = 0,
        .ser_opts.response_tout_ms = 1000,
        .ser_opts.data_bits = UART_DATA_8_BITS,
        .ser_opts.stop_bits = UART_STOP_BITS_1
    };

    esp_err_t err = mbc_master_create_serial(&comm, &modbus_handle);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG,"mb controller initialization fail, returns(0x%x).", (int)err);
    }

    // Set UART pin numbers
    err = uart_set_pin(MB_PORT_NUM, MB_TXD_PIN, MB_RXD_PIN,
                              MB_RE_PIN, UART_PIN_NO_CHANGE);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG,"uart_set_pin fail, returns(0x%x).", (int)err);
    }

    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG,"uart_set_mode fail, returns(0x%x).", (int)err);
    }

    vTaskDelay(5);
    err = mbc_master_set_descriptor(modbus_handle, &device_parameters[0], num_device_parameters);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG,"mbc_master_set_descriptor fail, returns(0x%x).", (int)err);
    }

    err = mbc_master_start(modbus_handle);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG,"mbc_master_start fail, returns(0x%x).", (int)err);
    }
}