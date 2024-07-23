#include "wifi_api.h"
#include "iot_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "heater_rinnai_api.h"

EventGroupHandle_t tcp_event;

/*tcp事件发送*/
void tcp_event_send(uint32_t event)
{
	xEventGroupSetBits(tcp_event,event);
}

/*tcp事件接收*/
uint32_t tcp_event_recv(uint32_t event,uint32_t timeout)
{
	return xEventGroupWaitBits(tcp_event,event,pdTRUE,pdFALSE,timeout);
}

void tcp_event_process(void *parameter)
{
	uint32_t event = 0;
	while(1)
	{
		event = tcp_event_recv(TCP_EVENT_LINK_UP | TCP_EVENT_LINK_DOWN ,portMAX_DELAY);
		if(event & TCP_EVENT_LINK_UP)
		{
			printf("TCP_EVENT_LINK_UP\r\n");
			wifi_heater_common_key_request();//heart
		}
		else if(event & TCP_EVENT_LINK_DOWN)
		{
			printf("TCP_EVENT_LINK_DOWN\r\n");
			vTaskDelay(pdMS_TO_TICKS(1000));
			tcp_event_send(TCP_CONNECT_RESET);//发送重连请求
		}
	}
}

void tcp_event_init(void)
{
	heater_heart_timer_init();
	heater_poll_timer_init();
	heater_detect_timer_init();
	wifi_rinnai_priority_timer_init();
	tcp_event = xEventGroupCreate();
	xTaskCreatePinnedToCore(tcp_event_process, "tcp_event", 4096, NULL, 3, NULL, tskNO_AFFINITY);
}
