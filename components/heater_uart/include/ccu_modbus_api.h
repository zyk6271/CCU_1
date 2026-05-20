/**
 * @file ccu_modbus_api.h
 * @brief Modbus 主站 API (V2.1 Rev.5)
 */
#ifndef CCU_MODBUS_API_H_
#define CCU_MODBUS_API_H_

#include <stdint.h>
#include "ccu_modbus_config.h"

typedef struct {
    uint8_t  enabled;
    uint8_t  status;        /* 0=空闲 1=OK 2=超时 3=CRC 4=异常 */
    uint32_t ok_count;
    uint32_t err_count;
    uint16_t reg_start;
    uint16_t reg_count;
} ccu_modbus_device_status_t;

#define MODBUS_LOG_ENTRY_MAX  20
#define MODBUS_LOG_NAME_MAX   24

typedef struct {
    uint32_t timestamp;
    char     device_name[MODBUS_LOG_NAME_MAX];
    uint8_t  result;        /* 0=OK 1=超时 2=CRC 3=异常 */
    uint8_t  slave_addr;
    uint8_t  operation;     /* 0=读保持 1=读输入 2=读离散 3=写保持 */
    uint16_t reg_start;
    uint16_t reg_count;
} ccu_modbus_log_entry_t;

/* ---- 初始化 / 轮询 ---- */
void    ccu_modbus_init(void);
void    ccu_modbus_poll(void);
void    ccu_poll_status_reset(void);
uint8_t modbus_detect_result_read(void);
void    wifi_ccu_modbus_poll_upload(void);

/* ---- 各设备 TCP 上报 (根据 encrypt_enabled 决定是否加密) ---- */
void gas_flow_sensor_info_upload(void);
void fridge_info_upload(void);
void currentwatch_info_upload(void);
void drycontact_sensor_info_upload(void);
void dish_washer_info_upload(void);
void gas_concentration_sensor_info_upload(void);
void ultra_sonic_gas_meter_info_upload(void);
void gas_steamer_info_upload(void);
void gas_wok_info_upload(void);

/* ---- Rev.3 设备管理接口 ---- */
void        ccu_modbus_device_status_get_all(ccu_modbus_device_status_t *out);
const char *ccu_modbus_device_name_get(uint8_t index);
uint8_t     ccu_modbus_device_addr_get(uint8_t index);
int         ccu_modbus_device_enable_set(uint8_t index, uint8_t enable);
int         ccu_modbus_device_reg_addr_set(uint8_t index, uint16_t reg_start, uint16_t reg_count);
int         ccu_modbus_device_slave_addr_set(uint8_t index, uint8_t slave_addr);
uint8_t     ccu_modbus_log_read(ccu_modbus_log_entry_t *out, uint8_t max_cnt);

/* 轮询间隔 (Rev.4) */
uint32_t    ccu_modbus_poll_interval_get(void);           /* 返回当前间隔 ms */
int         ccu_modbus_poll_interval_set(uint32_t ms);    /* 500~10000ms, 持久化, 立即生效 */

#endif