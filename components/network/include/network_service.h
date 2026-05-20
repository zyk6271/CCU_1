/**
 * @file network_service.h
 * @brief 网络服务管理器 - 统一入口 (MQTT + TCP + SNTP)
 *
 * 职责:
 *   1. 服务使能标志的 NVS 持久化 (MQTT/TCP 开关)
 *   2. SNTP 时间同步
 *   3. 全部连接状态聚合查询 (BLE CMD 0x05)
 *   4. 统一初始化 / 恢复出厂设置
 *
 * 各子服务具体实现:
 *   MQTT → mqtt_service.h / mqtt_service.c
 *   TCP  → tcp_service.h  / tcp_service.c
 */

#ifndef NETWORK_SERVICE_H_
#define NETWORK_SERVICE_H_

#include <stdint.h>
#include <stdbool.h>
#include "mqtt_service.h"
#include "tcp_service.h"

/* ============================================================ */
/* WiFi 详细状态 (与 BLE 协议 CMD 0x05 D0 对应)                    */
/* ============================================================ */

typedef enum
{
    WIFI_STATUS_IDLE              = 0x00,
    WIFI_STATUS_CONNECTING        = 0x01,
    WIFI_STATUS_CONNECTED         = 0x02,
    WIFI_STATUS_PASSWORD_ERROR    = 0x03,
    WIFI_STATUS_AP_NOT_FOUND      = 0x04,
    WIFI_STATUS_AUTH_FAIL         = 0x05,
    WIFI_STATUS_CONNECT_TIMEOUT   = 0x06,
    WIFI_STATUS_SMARTCONFIG       = 0x07,
    WIFI_STATUS_BLE_CONFIGURING   = 0x08,
} wifi_status_t;

/* ============================================================ */
/* Modbus 通讯状态 (BLE CMD 0x05 D6)                              */
/* ============================================================ */

typedef enum
{
    MODBUS_STATUS_IDLE            = 0x00,
    MODBUS_STATUS_OK              = 0x01,
    MODBUS_STATUS_TIMEOUT         = 0x02,
    MODBUS_STATUS_CRC_ERROR       = 0x03,
    MODBUS_STATUS_EXCEPTION       = 0x04,
} modbus_status_t;

/* ============================================================ */
/* SNTP 同步状态 (BLE CMD 0x05 D14)                               */
/* ============================================================ */

typedef enum
{
    SNTP_SYNC_NONE                = 0x00,
    SNTP_SYNC_COMPLETED           = 0x01,
    SNTP_SYNC_IN_PROGRESS         = 0x02,
    SNTP_SYNC_FAILED              = 0x03,
} sntp_status_t;

/* ============================================================ */
/* 全部连接状态 (BLE CMD 0x05 应答, 17B)                           */
/* ============================================================ */

typedef struct
{
    uint8_t  wifi_status;           /* D0  */
    int8_t   wifi_rssi;             /* D1  */
    uint8_t  mqtt_enabled;          /* D2  */
    uint8_t  mqtt_status;           /* D3  */
    uint8_t  tcp_enabled;           /* D4  */
    uint8_t  tcp_status;            /* D5  */
    uint8_t  modbus_status;         /* D6  */
    uint32_t mqtt_last_report_time; /* D7-D10 */
    uint8_t  tcp_key_configured;    /* D11 */
    uint8_t  tcp_config_status;     /* D12 */
    uint8_t  sntp_enabled;          /* D13 */
    uint8_t  sntp_sync_status;      /* D14 */
    uint8_t  tcp_server_mode;       /* D15 */
    uint8_t  tcp_encrypt_enabled;   /* D16 */
} all_connection_status_t;

/* ============================================================ */
/* 统一接口                                                       */
/* ============================================================ */

void network_service_init(void);

/* 使能标志持久化 */
int  network_service_save_enable_flags(uint8_t mqtt_en, uint8_t tcp_en);

/* 聚合状态查询 */
all_connection_status_t network_all_status_get(void);

/* SNTP */
void    network_sntp_enable_set(uint8_t enable);
uint8_t network_sntp_enable_get(void);
uint8_t network_sntp_sync_status_get(void);

/* 恢复出厂设置 */
void network_factory_reset(void);
void network_factory_reset_prepare(void);

#endif /* NETWORK_SERVICE_H_ */
