/**
 * @file iot_event.c
 * @brief TCP 内部事件 (EventGroup wrapper)
 *
 * 简化说明:
 *   原有 tcp_event_task 监听 LINK_UP/LINK_DOWN, 但:
 *   - LINK_UP 只触发 wifi_ccu_modbus_poll_upload(), 已改为 tcp_service 的回调
 *   - LINK_DOWN 从未被任何模块 send, 是死代码
 *   因此移除 task, 仅保留 EventGroup 供 TCP_CONNECT_RESET 使用.
 */

#include "iot_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static EventGroupHandle_t s_tcp_event_group;

void tcp_event_send(uint32_t event)
{
    if (s_tcp_event_group)
    {
        xEventGroupSetBits(s_tcp_event_group, event);
    }
}

uint32_t tcp_event_recv(uint32_t event, uint32_t timeout)
{
    return xEventGroupWaitBits(s_tcp_event_group, event, pdTRUE, pdFALSE, timeout);
}

void tcp_event_init(void)
{
    s_tcp_event_group = xEventGroupCreate();
}
