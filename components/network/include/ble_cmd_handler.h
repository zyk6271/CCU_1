/**
 * @file ble_cmd_handler.h
 * @brief BLE 指令处理 (Modbus 专用 V2.1 Rev.3)
 */

#ifndef BLE_CMD_HANDLER_H_
#define BLE_CMD_HANDLER_H_

#include <stdint.h>

#define BLE_CMD_MQTT_SWITCH              0x01
#define BLE_CMD_TCP_SWITCH               0x02
#define BLE_CMD_READ_MQTT_CONFIG         0x03
#define BLE_CMD_WRITE_MQTT_CONFIG        0x04
#define BLE_CMD_READ_ALL_STATUS          0x05
#define BLE_CMD_READ_MQTT_REPORT_INTV    0x06
#define BLE_CMD_WRITE_MQTT_REPORT_INTV   0x07
#define BLE_CMD_READ_DEVICE_INFO         0x08
#define BLE_CMD_READ_TCP_CONFIG          0x09
#define BLE_CMD_WRITE_TCP_CONFIG         0x0A
#define BLE_CMD_WIFI_CONFIG              0x0B
#define BLE_CMD_SMARTCONFIG_START        0x0C
#define BLE_CMD_DEVICE_RESTART           0x0D
#define BLE_CMD_FACTORY_RESET            0x0E
#define BLE_CMD_READ_TCP_REPORT_INTV     0x10
#define BLE_CMD_WRITE_TCP_REPORT_INTV    0x11

/* ---- Rev.3 新增指令 ---- */
#define BLE_CMD_READ_MODBUS_DEV_STATUS   0x20  /* 读取所有 Modbus 设备状态 */
#define BLE_CMD_WRITE_MODBUS_DEV_ENABLE  0x21  /* 设置 Modbus 设备通讯使能 */
#define BLE_CMD_READ_MODBUS_LOG          0x22  /* 读取 Modbus 通讯日志 */
#define BLE_CMD_WRITE_MODBUS_DEV_ADDR    0x23  /* 修改 Modbus 设备读取地址 */
#define BLE_CMD_WRITE_MODBUS_SLAVE_ADDR  0x24  /* 修改 Modbus 设备从机地址 */
#define BLE_CMD_SNTP_SWITCH              0x25  /* SNTP 对时开关 */
#define BLE_CMD_READ_SNTP_STATUS         0x26  /* 读取 SNTP 配置状态 (Rev.4) */
#define BLE_CMD_READ_WIFI_CONFIG         0x27  /* 读取当前 WiFi 配置 (Rev.4) */
#define BLE_CMD_READ_MODBUS_POLL_INTV    0x28  /* 读取 Modbus 轮询间隔 (Rev.4) */
#define BLE_CMD_WRITE_MODBUS_POLL_INTV   0x29  /* 写入 Modbus 轮询间隔 (Rev.4) */
#define BLE_CMD_READ_MQTT_RECENT         0x2A  /* 读取最近 MQTT 上报报文 (Rev.4) */

#define BLE_CMD_OTA_START                0x70
#define BLE_CMD_OTA_DATA                 0x71
#define BLE_CMD_OTA_EXECUTE              0x72

#define BLE_FRAME_HEAD  0x68
#define BLE_FRAME_TAIL  0x16

void ble_cmd_on_data_received(const uint8_t *data, uint16_t len);
void ble_cmd_send_response(uint8_t cmd, const uint8_t *data, uint16_t len);
void ble_cmd_handler_init(void);

#endif