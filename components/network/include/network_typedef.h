/**
 * @file network_typedef.h
 * @brief 通用类型定义与常量 (WiFi 协议层)
 *
 * 注: 仅保留项目实际使用的定义, 已移除涂鸦SDK遗留的未使用宏.
 *     如有 system.h / mcu_api.h 中引用的宏找不到, 请检查是否已迁移.
 */

#ifndef __WIFI_H_
#define __WIFI_H_

#include <stdlib.h>
#include <stdio.h>

/* ============================================================ */
/* 通用常量                                                       */
/* ============================================================ */

#ifndef TRUE
#define TRUE    1
#endif

#ifndef FALSE
#define FALSE   0
#endif

#ifndef NULL
#define NULL    ((void *) 0)
#endif

#ifndef ENABLE
#define ENABLE  1
#endif

#ifndef DISABLE
#define DISABLE 0
#endif

/* ============================================================ */
/* 字节序工具                                                      */
/* ============================================================ */

#ifndef WORD_SWAP
#define WORD_SWAP(X)    (((X << 8) | (X >> 8)) & 0xFFFF)
#endif

/* ============================================================ */
/* 数据点类型 (Modbus 上报协议)                                    */
/* ============================================================ */

#define DP_TYPE_RAW     0x00
#define DP_TYPE_BOOL    0x01
#define DP_TYPE_VALUE   0x02
#define DP_TYPE_STRING  0x03
#define DP_TYPE_ENUM    0x04
#define DP_TYPE_FAULT   0x05

/* ============================================================ */
/* WiFi 工作状态 (协议层, 与 wifi_manager 的 wifi_status_t 不同)   */
/* ============================================================ */

#define SMART_CONFIG_STATE      0x00
#define AP_STATE                0x01
#define WIFI_NOT_CONNECTED      0x02
#define WIFI_CONNECTED          0x03
#define WIFI_CONN_CLOUD         0x04
#define WIFI_LOW_POWER          0x05
#define SMART_AND_AP_STATE      0x06
#define WIFI_SATE_UNKNOW        0xff

/* ============================================================ */
/* 缓冲区大小                                                     */
/* ============================================================ */

#define BUF_SIZE    50

/* ============================================================ */
/* 依赖头文件                                                      */
/* ============================================================ */

#include "system.h"
#include "mcu_api.h"

#endif /* __WIFI_H_ */
