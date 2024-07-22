#ifndef __IOT_EVENT_H_
#define __IOT_EVENT_H_

#include <stdint.h>

#define TCP_EVENT_WIFI_CONNECTED		1 << 0 //wifi连接成功
#define TCP_EVENT_WIFI_DISCONNECTED		1 << 1 //wifi断开连接
#define TCP_EVENT_LINK_UP               1 << 2 //网络连接成功
#define TCP_EVENT_LINK_DOWN             1 << 3 //网络断开连接

#define TCP_CONNECT_RESET               1 << 4 //网络断开连接

void tcp_event_init(void);
void tcp_event_send(uint32_t event);
uint32_t tcp_event_recv(uint32_t event,uint32_t timeout);

#endif
