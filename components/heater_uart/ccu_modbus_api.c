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
    GAS_FLOW_SENSOR_MB_ADDR = 0x01,
    FRIDGE_MB_ADDR = 0x02,
    CURRENTWATCH_MB_ADDR = 0x03,
    DRYCONTACT_MB_ADDR = 0x04,
    DISHWASHER_MB_ADDR = 0x08,
    GAS_CONCENTRATION_SENSOR_MB_ADDR = 0x09,
};

// Enumeration of all supported CIDs for device (used in parameter definition table)
enum {
    CID_GAS_FLOW_SENSOR = 0,
    CID_FRIDGE,
    CID_CURRENTWATCH,
    CID_DRYCONTACT,
    CID_DISHWASHER,
    CID_GAS_CONCENTRATION_SENSOR,
    CID_COUNT
};

uint8_t modbus_detect_result = 0;

uint8_t modbus_currentwatch_value[16] = {0};             
uint8_t modbus_fridge_value[64] = {0};
uint8_t modbus_gas_flow_sensor_value[32] = {0};                     
uint8_t modbus_dishwasher_value[128] = {0};
uint8_t modbus_drycontact_value[16] = {0};
uint8_t modbus_gas_concentration_sensor_value[32] = {0};

uint8_t modbus_read_temp[128] = {0};   

extern uint8_t smartconfig_start_flag;    

const mb_parameter_descriptor_t device_parameters[] = {
    {
        CID_GAS_FLOW_SENSOR,
        "GAS_FLOW_SENSOR",        // 名称
        "--",                    // 无单位
        GAS_FLOW_SENSOR_MB_ADDR,  // 从机地址（需与设备一致）
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
    {
        CID_GAS_CONCENTRATION_SENSOR,
        "GAS_CONCENTRATION_SENSOR",             // 名称
        "--",                                   // 无单位
        GAS_CONCENTRATION_SENSOR_MB_ADDR,       // 从机地址（需与设备一致）
        MB_PARAM_HOLDING,                       // 输入寄存器（只读）
        0x0016,                                 // 寄存器起始地址
        10,                                     // 占用10个寄存器（16位）
        0,                                      // 数据偏移量（需根据实际结构体调整）
        PARAM_TYPE_U16_BA,                      // 16位无符号整数
        20,                                     // 84字节
        OPTS(0, 0, 0),                          // 无范围限制
        PAR_PERMS_READ                          // 只读
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
    plain_buf[0] = tcp_send_count_read();
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
    plain_buf[0] = tcp_send_count_read();
    memcpy(&plain_buf[1],  modbus_fridge_value, 21);  // 0x1321（2字节）

    ESP_LOG_BUFFER_HEXDUMP("fridge_info_upload", plain_buf, 41, ESP_LOG_INFO);

    crypto_aes_remote_encrypt(plain_buf, 41, &encrypt_ptr, &encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xB0, 41, send_len);

    free(encrypt_ptr);
}

void gas_flow_sensor_info_upload(void)
{
    uint8_t plain_buf[41] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    static int total_flow,total_flow_past = 0;
    total_flow = (modbus_gas_flow_sensor_value[6] * 0x100000) + (modbus_gas_flow_sensor_value[7] * 0x10000) + (modbus_gas_flow_sensor_value[8] * 0x1000) + (modbus_gas_flow_sensor_value[9] * 0x100) + (modbus_gas_flow_sensor_value[10] * 0x10) + modbus_gas_flow_sensor_value[11];
    if(total_flow_past == 0 && total_flow != total_flow_past)
    {
        total_flow_past = total_flow;
    }

    // 按寄存器地址分段拷贝（单位：字节）
    plain_buf[0] = tcp_send_count_read();
    memcpy(&plain_buf[1],  modbus_gas_flow_sensor_value, 26); 
    plain_buf[30] = (((uint16_t)(((total_flow - total_flow_past) * 0.135) ) % 0xFFFF) >> 8) & 0xFF;
    plain_buf[31] = ((uint16_t)(((total_flow - total_flow_past) * 0.135) ) % 0xFFFF ) & 0xFF;

    ESP_LOG_BUFFER_HEXDUMP("gas_flow_info_upload", plain_buf, 41, ESP_LOG_INFO);

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
    plain_buf[0] = tcp_send_count_read();
    memcpy(&plain_buf[1],  modbus_drycontact_value, 1); 

    ESP_LOG_BUFFER_HEXDUMP("drycontact_info_upload", plain_buf, 41, ESP_LOG_INFO);

    crypto_aes_remote_encrypt(plain_buf, 41, &encrypt_ptr, &encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xB0, 41, send_len);

    free(encrypt_ptr);
}

void gas_concentration_sensor_info_upload(void)
{
    uint8_t plain_buf[41] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    // 按寄存器地址分段拷贝（单位：字节）
    plain_buf[0] = tcp_send_count_read();
    memcpy(&plain_buf[1],  modbus_gas_concentration_sensor_value + (0x0016 - 0x0016) * 2, 2);  // 0x1321（2字节）
    memcpy(&plain_buf[3],  modbus_gas_concentration_sensor_value + (0x001E - 0x0016) * 2, 4);  // 0x1324~0x1327（4寄存器 × 2 = 8字节）

    ESP_LOG_BUFFER_HEXDUMP("gas_concentration_sensor_info_upload", plain_buf, 41, ESP_LOG_INFO);

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
    memset(modbus_gas_flow_sensor_value,0,sizeof(modbus_gas_flow_sensor_value));
    memset(modbus_drycontact_value,0,sizeof(modbus_drycontact_value));
    memset(modbus_gas_concentration_sensor_value,0,sizeof(modbus_gas_concentration_sensor_value));
}

void ccu_modbus_poll_select(uint8_t modbus_cid,uint8_t* value_temp,void (*mb_callback)(void))
{
    uint8_t type = 0;  
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;
    err = mbc_master_get_cid_info(ccu_modbus_handle, modbus_cid, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) 
    {
        memset(modbus_read_temp, 0, sizeof(modbus_read_temp));
        err = mbc_master_get_parameter(ccu_modbus_handle, param_descriptor->cid, modbus_read_temp, &type);
        if (err == ESP_OK) 
        {
            modbus_detect_result = 1;
            if(memcmp(value_temp,modbus_read_temp,param_descriptor->param_size) == 0)
            {
                ESP_LOGI(TAG, "%s modbus value no change\r\n",param_descriptor->param_key);
            }
            else
            {
                memcpy(value_temp,modbus_read_temp,param_descriptor->param_size);
                ESP_LOGI(TAG, "%s modbus value has change\r\n",param_descriptor->param_key);
                //ESP_LOG_BUFFER_HEXDUMP("modbus_recv_buf", modbus_read_temp, param_descriptor->param_size, ESP_LOG_INFO);
                mb_callback();
            }
        } 
    } 
}

void ccu_modbus_poll(void)
{
    if(smartconfig_start_flag != 0)
    {
        return;
    }

    ccu_modbus_poll_select(CID_GAS_FLOW_SENSOR,modbus_gas_flow_sensor_value,gas_flow_sensor_info_upload);
    ccu_modbus_poll_select(CID_FRIDGE,modbus_fridge_value,fridge_info_upload);
    ccu_modbus_poll_select(CID_CURRENTWATCH,modbus_currentwatch_value,currentwatch_info_upload);
    ccu_modbus_poll_select(CID_DRYCONTACT,modbus_drycontact_value,drycontact_sensor_info_upload);
    ccu_modbus_poll_select(CID_DISHWASHER,modbus_dishwasher_value,dish_washer_info_upload);
    ccu_modbus_poll_select(CID_GAS_CONCENTRATION_SENSOR,modbus_gas_concentration_sensor_value,gas_concentration_sensor_info_upload);
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