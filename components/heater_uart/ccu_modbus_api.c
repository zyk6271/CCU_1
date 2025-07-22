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
#include "ccu_modbus_api.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "string.h"
#include "crypto_aes.h"
#include "network_typedef.h"
#include "wifi_api.h"
#include "wifi_manager.h"
#include "heater_interface_api.h"

static const char *TAG = "ccu_modbus";

static void *ccu_modbus_handle = NULL;

#define MB_TXD_PIN (GPIO_NUM_21)
#define MB_RXD_PIN (GPIO_NUM_20)
#define MB_RE_PIN  (GPIO_NUM_10)

#define MB_PORT_NUM     (UART_NUM_0)   // Number of UART port used for Modbus connection
#define MB_DEV_SPEED    (9600)  // The communication speed of the UART

#define OPTS(min_val, max_val, step_val) { .opt1 = min_val, .opt2 = max_val, .opt3 = step_val }

// Enumeration of modbus device addresses accessed by master device
enum {
    GASFLOW_SENSOR_MB_ADDR = 0x01, // Only one slave device used for the test (add other slave addresses here)
    FRIDGE_MB_ADDR = 0x02, // Only one slave device used for the test (add other slave addresses here)
    CURRENTWATCH_MB_ADDR = 0x03, // Only one slave device used for the test (add other slave addresses here)
    DRYCONTACT_MB_ADDR = 0x04, // Only one slave device used for the test (add other slave addresses here)
    DISHWASHER_MB_ADDR = 0x08 // Only one slave device used for the test (add other slave addresses here)
};

// Enumeration of all supported CIDs for device (used in parameter definition table)
enum {
    CID_GASFLOW_SENSOR = 0,
    CID_FRIDGE ,
    CID_CURRENTWATCH  ,
    CID_DRYCONTACT ,
    CID_DISHWASHER ,
    CID_COUNT
};

uint8_t modbus_detect_result = 0;
uint8_t modbus_currentwatch_value[64] = {0};             
uint8_t modbus_fridge_value[64] = {0};
uint8_t modbus_gasflow_sensor_value[64] = {0};                     
uint8_t modbus_dishwasher_value[128] = {0};
uint8_t modbus_drycontact_value[16] = {0};
uint8_t modbus_read_temp[128] = {0};   

extern uint8_t smartconfig_start_flag;    

// Access Mode - can be used to implement custom options for processing of characteristic (Read/Write restrictions, factory mode values and etc).
const mb_parameter_descriptor_t device_parameters[] = {
    {
        CID_GASFLOW_SENSOR,
        "GASFLOW_SENSOR",        // 名称
        "--",                    // 无单位
        GASFLOW_SENSOR_MB_ADDR,  // 从机地址（需与设备一致）
        MB_PARAM_HOLDING,        // 输入寄存器（只读）
        0x0001,                  // 寄存器起始地址
        13,                      // 占用12个寄存器（8位）
        0,                       // 数据偏移量（需根据实际结构体调整）
        PARAM_TYPE_U16_BA,       // 16位无符号整数
        26,                      // 24字节
        OPTS(0, 0, 0),           // 无范围限制
        PAR_PERMS_READ           // 只读
    },
    {
        CID_FRIDGE,
        "FRIDGE",                // 名称
        "--",                    // 无单位
        FRIDGE_MB_ADDR,          // 从机地址（需与设备一致）
        MB_PARAM_HOLDING,        // 输入寄存器（只读）
        0x0000,                  // 寄存器起始地址
        21,                      // 占用21个寄存器（8位）
        0,                       // 数据偏移量（需根据实际结构体调整）
        PARAM_TYPE_U16_BA,       // 16位无符号整数
        42,                      // 42字节
        OPTS(0, 0, 0),           // 无范围限制
        PAR_PERMS_READ           // 只读
    }, 
    {
        CID_CURRENTWATCH,
        "CURRENTWATCH",          // 名称
        "--",                    // 无单位
        CURRENTWATCH_MB_ADDR,    // 从机地址（需与设备一致）
        MB_PARAM_HOLDING,        // 输入寄存器（只读）
        0x0020,                  // 寄存器起始地址
        6,                       // 占用6个寄存器（16位）
        0,                       // 数据偏移量（需根据实际结构体调整）
        PARAM_TYPE_FLOAT_BADC,   // 16位无符号整数
        12,                      // 3字节
        OPTS(0, 0, 0),           // 无范围限制
        PAR_PERMS_READ           // 只读
    },
    {
        CID_DRYCONTACT,
        "DRYCONTACT",            // 名称
        "--",                    // 无单位
        DRYCONTACT_MB_ADDR,      // 从机地址（需与设备一致）
        MB_PARAM_DISCRETE,       // 输入寄存器（只读）
        0x0000,                  // 寄存器起始地址
        3,                       // 占用4个寄存器（16位）
        0,                       // 数据偏移量（需根据实际结构体调整）
        PARAM_TYPE_U8,           // 8位无符号整数
        1,                       // 1字节
        OPTS(0, 0, 0),           // 无范围限制
        PAR_PERMS_READ           // 只读
    },
    {
        CID_DISHWASHER,
        "DISHWASHER",            // 名称
        "--",                    // 无单位
        DISHWASHER_MB_ADDR,      // 从机地址（需与设备一致）
        MB_PARAM_HOLDING,        // 输入寄存器（只读）
        0x1320,                  // 寄存器起始地址
        42,                      // 占用44个寄存器（16位）
        0,                       // 数据偏移量（需根据实际结构体调整）
        PARAM_TYPE_U16_BA,       // 16位无符号整数
        84,                      // 84字节
        OPTS(0, 0, 0),           // 无范围限制
        PAR_PERMS_READ           // 只读
    },
};

const uint16_t num_device_parameters = (sizeof(device_parameters)/sizeof(device_parameters[0]));

uint8_t modbus_detect_result_read(void)
{
    return modbus_detect_result;
}

void dish_washer_info_upload(void)
{
    uint8_t plain_buf[41] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    // 按寄存器地址分段拷贝（单位：字节）
    plain_buf[0] = tcp_send_count_read();;
    memcpy(&plain_buf[1],  modbus_dishwasher_value + (0x1321 - 0x1320) * 2, 2);  // 0x1321（2字节）
    memcpy(&plain_buf[3],  modbus_dishwasher_value + (0x1324 - 0x1320) * 2, 8);  // 0x1324~0x1327（4寄存器 × 2 = 8字节）
    memcpy(&plain_buf[11], modbus_dishwasher_value + (0x132C - 0x1320) * 2, 22); // 0x132C~0x1336（11寄存器 × 2 = 22字节）
    memcpy(&plain_buf[33], modbus_dishwasher_value + (0x133B - 0x1320) * 2, 6);  // 0x133B~0x133D（3寄存器 × 2 = 6字节）
    memcpy(&plain_buf[39], modbus_dishwasher_value + (0x1349 - 0x1320) * 2, 2);  // 0x1349（2字节）

    ESP_LOG_BUFFER_HEXDUMP("dish_washer_info_upload", plain_buf, 41, ESP_LOG_INFO);

    crypto_aes_remote_encrypt(plain_buf, 41, &encrypt_ptr, &encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xB0, 41, send_len);

    free(encrypt_ptr);
}

void currentwatch_info_upload(void)
{
    uint8_t plain_buf[41] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    // 按寄存器地址分段拷贝（单位：字节）
    plain_buf[0] = tcp_send_count_read();;
    memcpy(&plain_buf[1],  modbus_currentwatch_value, 12);  // 0x1321（2字节）

    ESP_LOG_BUFFER_HEXDUMP("currentwatch_info_upload", plain_buf, 41, ESP_LOG_INFO);

    crypto_aes_remote_encrypt(plain_buf, 41, &encrypt_ptr, &encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xB0, 41, send_len);

    free(encrypt_ptr);
}

void fridge_info_upload(void)
{
    uint8_t plain_buf[41] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    // 按寄存器地址分段拷贝（单位：字节）
    plain_buf[0] = tcp_send_count_read();;
    memcpy(&plain_buf[1],  modbus_fridge_value, 21);  // 0x1321（2字节）

    ESP_LOG_BUFFER_HEXDUMP("fridge_info_upload", plain_buf, 41, ESP_LOG_INFO);

    crypto_aes_remote_encrypt(plain_buf, 41, &encrypt_ptr, &encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xB0, 41, send_len);

    free(encrypt_ptr);
}

void gasflow_sensor_info_upload(void)
{
    uint8_t plain_buf[41] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    static int total_flow,total_flow_past = 0;
    total_flow = (modbus_gasflow_sensor_value[6] * 0x100000) + (modbus_gasflow_sensor_value[7] * 0x10000) + (modbus_gasflow_sensor_value[8] * 0x1000) + (modbus_gasflow_sensor_value[9] * 0x100) + (modbus_gasflow_sensor_value[10] * 0x10) + modbus_gasflow_sensor_value[11];
    if(total_flow_past == 0 && total_flow != total_flow_past)
    {
        total_flow_past = total_flow;
    }

    // 按寄存器地址分段拷贝（单位：字节）
    plain_buf[0] = tcp_send_count_read();;
    memcpy(&plain_buf[1],  modbus_gasflow_sensor_value, 26); 
    plain_buf[30] = (((uint16_t)(((total_flow - total_flow_past) * 0.135) ) % 0xFFFF) >> 8) & 0xFF;
    plain_buf[31] = ((uint16_t)(((total_flow - total_flow_past) * 0.135) ) % 0xFFFF ) & 0xFF;

    ESP_LOG_BUFFER_HEXDUMP("gasflow_info_upload", plain_buf, 41, ESP_LOG_INFO);

    crypto_aes_remote_encrypt(plain_buf, 41, &encrypt_ptr, &encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xB0, 41, send_len);

    free(encrypt_ptr);
}

void drycontact_sensor_info_upload(void)
{
    uint8_t plain_buf[41] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    // 按寄存器地址分段拷贝（单位：字节）
    plain_buf[0] = tcp_send_count_read();;
    memcpy(&plain_buf[1],  modbus_drycontact_value, 1); 

    ESP_LOG_BUFFER_HEXDUMP("drycontact_info_upload", plain_buf, 41, ESP_LOG_INFO);

    crypto_aes_remote_encrypt(plain_buf, 41, &encrypt_ptr, &encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xB0, 41, send_len);

    free(encrypt_ptr);
}

void ccu_poll_status_reset(void)
{
    memset(modbus_currentwatch_value,0,sizeof(modbus_currentwatch_value));
    memset(modbus_fridge_value,0,sizeof(modbus_fridge_value));
    memset(modbus_dishwasher_value,0,sizeof(modbus_dishwasher_value));
}
    
void ccu_modbus_poll(void)
{
    uint8_t type = 0;  
    esp_err_t err = ESP_OK;

    if(smartconfig_start_flag != 0)
    {
        return;
    }

    const mb_parameter_descriptor_t* param_descriptor = NULL;
    err = mbc_master_get_cid_info(ccu_modbus_handle, CID_CURRENTWATCH, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) 
    {
        memset(modbus_read_temp, 0, sizeof(modbus_read_temp));
        err = mbc_master_get_parameter(ccu_modbus_handle, param_descriptor->cid, modbus_read_temp, &type);
        if (err == ESP_OK) 
        {
            modbus_detect_result = 1;
            if(memcmp(modbus_currentwatch_value,modbus_read_temp,sizeof(modbus_currentwatch_value)) == 0)
            {
                ESP_LOGI(TAG, "modbus_currentwatch value no change\r\n");
            }
            else
            {
                memcpy(modbus_currentwatch_value,modbus_read_temp,sizeof(modbus_currentwatch_value));
                ESP_LOGI(TAG, "modbus_currentwatch value has change\r\n");
                ESP_LOG_BUFFER_HEXDUMP("modbus_currentwatch_buf", modbus_read_temp, 64, ESP_LOG_INFO);
                currentwatch_info_upload();
            }
        } 
    } 
    vTaskDelay(pdMS_TO_TICKS(300));

    err = mbc_master_get_cid_info(ccu_modbus_handle, CID_FRIDGE, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) 
    {
        memset(modbus_read_temp, 0, sizeof(modbus_read_temp));
        err = mbc_master_get_parameter(ccu_modbus_handle, param_descriptor->cid, modbus_read_temp, &type);
        if (err == ESP_OK) 
        {
            modbus_detect_result = 1;
            if(memcmp(modbus_fridge_value,modbus_read_temp,sizeof(modbus_fridge_value)) == 0)
            {
                ESP_LOGI(TAG, "modbus_fridge value no change\r\n");
            }
            else
            {
                memcpy(modbus_fridge_value,modbus_read_temp,sizeof(modbus_fridge_value));
                ESP_LOGI(TAG, "modbus_fridge value has change\r\n");
                ESP_LOG_BUFFER_HEXDUMP("modbus_fridge_buf", modbus_read_temp, 64, ESP_LOG_INFO);
                fridge_info_upload();
            }
        } 
    } 
    vTaskDelay(pdMS_TO_TICKS(300));
    
    err = mbc_master_get_cid_info(ccu_modbus_handle, CID_GASFLOW_SENSOR, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) 
    {
        memset(modbus_read_temp, 0, sizeof(modbus_read_temp));
        err = mbc_master_get_parameter(ccu_modbus_handle, param_descriptor->cid, modbus_read_temp, &type);
        if (err == ESP_OK) 
        {
            modbus_detect_result = 1;
            if(memcmp(modbus_gasflow_sensor_value,modbus_read_temp,sizeof(modbus_gasflow_sensor_value)) == 0)
            {
                ESP_LOGI(TAG, "modbus_gasflow_sensor value no change\r\n");
            }
            else
            {
                memcpy(modbus_gasflow_sensor_value,modbus_read_temp,sizeof(modbus_gasflow_sensor_value));
                ESP_LOGI(TAG, "modbus_gasflow_sensor value has change\r\n");
                ESP_LOG_BUFFER_HEXDUMP("modbus_gasflow_sensor_buf", modbus_read_temp, 64, ESP_LOG_INFO);
                gasflow_sensor_info_upload();
            }
        } 
    } 
    vTaskDelay(pdMS_TO_TICKS(300));

    err = mbc_master_get_cid_info(ccu_modbus_handle, CID_DRYCONTACT, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) 
    {
        memset(modbus_read_temp, 0, sizeof(modbus_read_temp));
        err = mbc_master_get_parameter(ccu_modbus_handle, param_descriptor->cid, modbus_read_temp, &type);
        if (err == ESP_OK) 
        {
            modbus_detect_result = 1;
            if(memcmp(modbus_drycontact_value,modbus_read_temp,sizeof(modbus_drycontact_value)) == 0)
            {
                ESP_LOGI(TAG, "modbus_drycontact value no change\r\n");
            }
            else
            {
                memcpy(modbus_drycontact_value,modbus_read_temp,sizeof(modbus_drycontact_value));
                ESP_LOGI(TAG, "modbus_drycontact value has change\r\n");
                drycontact_sensor_info_upload();
            }
        } 
        ESP_LOG_BUFFER_HEXDUMP("modbus_drycontact_buf", modbus_read_temp, 64, ESP_LOG_INFO);
    } 
    vTaskDelay(pdMS_TO_TICKS(300));

    err = mbc_master_get_cid_info(ccu_modbus_handle, CID_DISHWASHER, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) 
    {
        memset(modbus_read_temp, 0, sizeof(modbus_read_temp));
        err = mbc_master_get_parameter(ccu_modbus_handle, param_descriptor->cid, modbus_read_temp, &type);
        if (err == ESP_OK) 
        {
            modbus_detect_result = 1;
            if(memcmp(modbus_dishwasher_value,modbus_read_temp,sizeof(modbus_dishwasher_value)) == 0)
            {
                ESP_LOGI(TAG, "modbus_dishwasher value no change\r\n");
            }
            else
            {
                memcpy(modbus_dishwasher_value,modbus_read_temp,sizeof(modbus_dishwasher_value));
                ESP_LOGI(TAG, "modbus_dishwasher value has change\r\n");
                ESP_LOG_BUFFER_HEXDUMP("modbus_dishwasher_buf", modbus_read_temp, 128, ESP_LOG_INFO);
                dish_washer_info_upload();
            }
        } 
    } 
}

void hmodbus_poll_thread_callback(void *parameter)
{
    while(1)
    {
        ccu_modbus_poll();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void ccu_modbus_init(void)
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

    esp_err_t err = mbc_master_create_serial(&comm, &ccu_modbus_handle);
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
    err = mbc_master_set_descriptor(ccu_modbus_handle, &device_parameters[0], num_device_parameters);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG,"mbc_master_set_descriptor fail, returns(0x%x).", (int)err);
    }

    err = mbc_master_start(ccu_modbus_handle);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG,"mbc_master_start fail, returns(0x%x).", (int)err);
    }

    xTaskCreate(hmodbus_poll_thread_callback, "modbus_poll_thread_handle", 8192, NULL, 5, NULL);
}