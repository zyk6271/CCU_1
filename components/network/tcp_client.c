#include "sdkconfig.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>            
#include <arpa/inet.h>
#include "esp_netif.h"
#include "esp_log.h"
#include "iot_event.h"
#include "wifi_service.h"
#include "tcp_client.h"

static const char *TAG = "tcp_client";

#define HOST_BUFSZ	 1024
//#define HOST_IP     "smartdevice-uat.towngas-uat.com"
 #define HOST_IP     "smartdevice-dev.towngas.com"
//#define HOST_IP     "smartdevice.towngas.com"
#define HOST_PORT    10020

static int already_connected = 0;
static int sock = -1;

void close_socket(void)
{
	if(sock >= 0)
	{
		closesocket(sock);
		sock = -1;
		already_connected = 0;
	}
}

void tcp_client_entry(void* parameter)
{
    int error_count = 0;
    uint8_t *recv_data;
    int bytes_received;
    struct sockaddr_in server_addr;
    struct hostent *host;
    struct timeval timeout;
	fd_set readset;

	/*设置超时时间*/
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;

    recv_data = calloc(1, HOST_BUFSZ);
    if (recv_data == NULL)
    {
        ESP_LOGE(TAG,"No memory");
        return;
    }
	ESP_LOGI(TAG,"tcp_client_entry init ok");

    tcp_event_recv(TCP_EVENT_WIFI_CONNECTED,portMAX_DELAY);
	ESP_LOGI(TAG,"__WAIT_WIFI ok");

__CONNECT:
    /* 通过函数入口参数url获得host地址（如果是域名，会做域名解析） */
    host = gethostbyname(HOST_IP);
	if (host == NULL)
    {
		ESP_LOGE(TAG,"gethostbyname fail");
		goto __RESET_CONNECT;
	}
	else
	{
		/* 初始化预连接的服务端地址 */
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(HOST_PORT);
		server_addr.sin_addr = *((struct in_addr *)host->h_addr);
		memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));
	}

    /* 创建一个socket，类型是SOCKET_STREAM，TCP 协议 */
	sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock< 0)
    {
        ESP_LOGE(TAG,"Socket error");
        goto __RESET_CONNECT;
    }

	while(1)
	{		
		FD_ZERO(&readset);
		if(already_connected == 1)
		{		
			if(tcp_event_recv(TCP_CONNECT_RESET,0) & TCP_CONNECT_RESET)
			{
				ESP_LOGE(TAG,"接收到tcp重连请求事件!!!");
				goto __RESET_CONNECT;
			}
			if(sock < 0)
			{
				ESP_LOGE(TAG,"需要重连tcp!!!");
				goto __RESET_CONNECT;
			}
			//要保证sock是>= 0
			FD_SET(sock, &readset);
			/* Wait for read */
			if (select(sock + 1, &readset, NULL, NULL, &timeout) == 0)
			{
        		vTaskDelay(pdMS_TO_TICKS(5));
				continue;
			}
			bytes_received = recv(sock, recv_data, HOST_BUFSZ - 1, 0);
			if(bytes_received <= 0)
			{
				ESP_LOGE(TAG,"服务端异常,断开重连 bytes_received=%d",bytes_received);
				goto __RESET_CONNECT;
			}
			else
			{
			    wifi_recv_buffer(recv_data,bytes_received);
				bytes_received = 0;
				continue;
			}
		}
		if(already_connected == 0 && connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
		{
			error_count ++;
			if(error_count >= 20)
			{
				ESP_LOGI(TAG,"连接服务器错误!!!");
				error_count = 0;
			}
        	vTaskDelay(pdMS_TO_TICKS(1000));
			goto __RESET_CONNECT;

		}
		else
		{
			error_count = 0;
			already_connected = 1;
			ESP_LOGI(TAG,"连接服务器成功!!");
			tcp_event_send(TCP_EVENT_LINK_UP);//设备上线
		}
        vTaskDelay(pdMS_TO_TICKS(200));
	}
__RESET_CONNECT:
	vTaskDelay(pdMS_TO_TICKS(3000));
	close_socket();
    goto __CONNECT;
}

uint8_t tcp_client_send(uint8_t *send_buf,size_t len)
{
	if(sock < 0 || already_connected == 0)
	{
		return -1;
	}

	int err = send(sock,send_buf,len,0);
	if(err < 0)
	{
		ESP_LOGE(TAG,"send fail,err code %d",err);
		tcp_event_send(TCP_CONNECT_RESET);
	}

	return 0;
}

void tcp_client_init(void)
{
    tcp_event_init();
    xTaskCreatePinnedToCore(tcp_client_entry, "tcp_client", 4096, NULL, 3, NULL, tskNO_AFFINITY);
}
