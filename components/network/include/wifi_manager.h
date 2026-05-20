/**
 * @file wifi_manager.h
 * @brief WiFi 管理 (BLE NUS 配网 + SmartConfig)
 */

#ifndef WIFI_MANAGER_H_
#define WIFI_MANAGER_H_

#include <stdint.h>

void wifi_interface_init(void);

/**
 * @brief  通过 BLE 下发 WiFi 凭证进行配网
 * @return 0=已开始连接, -1=参数错误
 */
int wifi_config_from_ble(const char *ssid, const char *password);

/**
 * @brief  触发 SmartConfig (ESPTouch V2, 3分钟超时)
 */
void smartconfig_start(void);

/**
 * @brief  停止 SmartConfig
 */
void smartconfig_stop(void);

/**
 * @brief  获取 WiFi 详细状态 (返回 wifi_status_t)
 */
uint8_t wifi_get_detail_status(void);

/**
 * @brief  获取 WiFi RSSI, 未连接时返回 0
 */
int8_t wifi_get_rssi(void);

#endif
