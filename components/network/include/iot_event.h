/**
 * @file iot_event.h
 * @brief TCP 内部事件 (仅供 tcp_service 内部使用)
 *
 * 历史遗留说明:
 *   原文件定义了 5 个事件, 实际仅 TCP_CONNECT_RESET 被双向使用.
 *   TCP_EVENT_LINK_UP 的 Modbus 上报功能已移至 tcp_service 回调机制.
 *   TCP_EVENT_WIFI_CONNECTED/DISCONNECTED, TCP_EVENT_LINK_DOWN 为死代码, 已移除.
 */

#ifndef __IOT_EVENT_H_
#define __IOT_EVENT_H_

#include <stdint.h>

/* ============================================================ */
/* 事件定义                                                       */
/* ============================================================ */

/**
 * TCP_CONNECT_RESET: tcp_task_entry 接收循环中检测到此事件时断开重连.
 * 发送方: tcp_service_send() 发送失败时
 */
#define TCP_CONNECT_RESET           (1 << 0)

/* ============================================================ */
/* 接口                                                           */
/* ============================================================ */

void     tcp_event_init(void);
void     tcp_event_send(uint32_t event);
uint32_t tcp_event_recv(uint32_t event, uint32_t timeout);

#endif /* __IOT_EVENT_H_ */
