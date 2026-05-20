/**
 * @file ble_uart.h
 * @brief ESP32 BLE UART 服务 (Nordic NUS 风格)
 */

#ifndef BLE_UART_H_
#define BLE_UART_H_

#include <stdint.h>
#include <stdbool.h>

#define BLE_UART_BUF_SIZE   1024
#define BLE_MTU_MAX         512    /* 设备端支持的最大 MTU */

/** @brief 初始化 BLE UART 服务 (NUS) */
void ble_uart_init(void);

/** @brief 是否有客户端连接 */
bool ble_uart_is_connected(void);

/** @brief 通过 TX Notify 发送数据 (自动按协商 MTU 分片) */
int ble_uart_send(const uint8_t *data, uint16_t len);

/** @brief 获取当前协商的 MTU (未连接时返回 23) */
uint16_t ble_uart_get_mtu(void);

/** @brief 重置 BLE 空闲断连计时器 */
void ble_uart_activity_reset(void);

/** @brief 主动断开当前 BLE 连接 */
void ble_uart_disconnect(void);

#endif