/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "iot_event.h"
#include "esp_smartconfig.h"
#include "storage.h"
#include "led.h"
#include "wifi_manager.h"
#include "led.h"

#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "wifi station";

uint8_t smartconfig_start_flag = 0;
static uint8_t smartconfig_retry_counter = 0;

static wifi_config_t wifi_config;

static SemaphoreHandle_t smartconfig_sem = NULL;
static esp_timer_handle_t smartconfig_wait_timer;

void smartconfig_stop(void)
{
    smartconfig_start_flag = 0;
    led_network_status_handle(0);
    esp_wifi_stop();
    esp_smartconfig_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_connect();
    ESP_LOGI(TAG, "esp_smartconfig_stop,start to connect router");
}

void smartconfig_wait_timer_callback(void* arg)
{
    smartconfig_stop();
}

void smartconfig_wait_timer_init(void)
{
    const esp_timer_create_args_t timer_args = 
    {
        .callback = &smartconfig_wait_timer_callback,
        .name = "smartconfig_wait_timer"
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &smartconfig_wait_timer));
}

void smartconfig_wait_timer_start(void)
{
    esp_timer_stop(smartconfig_wait_timer);
    esp_timer_start_once(smartconfig_wait_timer, 3 * 60 * 1000 * 1000);
}

void smartconfig_wait_timer_stop(void)
{
    esp_timer_stop(smartconfig_wait_timer);
}

void smartconfig_start(void)
{
    smartconfig_retry_counter = 0;
    smartconfig_start_flag = 1;
    led_network_status_handle(1);
    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    ESP_ERROR_CHECK( esp_smartconfig_stop() );
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2) );
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    smartconfig_wait_timer_start();
    ESP_LOGI(TAG, "esp_smartconfig_start");
}

void smartconfig_reset(void)
{
    xSemaphoreTake( smartconfig_sem,0 );
    smartconfig_start();
}

uint64_t smartconfig_sem_get(void)
{
    return xSemaphoreTake( smartconfig_sem,0 );
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        uint32_t config_flag;
        if(storage_read_key_value("wifi_mode",&config_flag) != ESP_OK)
        {
            config_flag = 0;
        }
        ESP_LOGI(TAG, "wifi_mode is %ld",config_flag);
        if(config_flag)
        {
            uint8_t ssid_temp[33];
            uint32_t ssid_length;
            uint8_t password_temp[33];
            uint32_t password_length;
            storage_read_key_blob("wifi_ssid",ssid_temp,&ssid_length);
            storage_read_key_blob("wifi_pwd",password_temp,&password_length);
            ESP_LOGI(TAG, "SSID:%s", ssid_temp);
            ESP_LOGI(TAG, "PASSWORD:%s", password_temp);
            bzero(&wifi_config, sizeof(wifi_config_t));
            memcpy(wifi_config.sta.ssid, ssid_temp, ssid_length);
            memcpy(wifi_config.sta.password, password_temp, password_length);
            ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
            ESP_LOGI(TAG, "esp_wifi_connect");
            esp_wifi_connect();
        }
        else
        {
            led_network_status_handle(0);
        }
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
        if(smartconfig_start_flag == 0)
        {
            led_network_status_handle(0);
            esp_wifi_stop();
            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_start();
            esp_wifi_connect();
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else if(smartconfig_start_flag == 1)
        {
            ESP_LOGI(TAG, "not allow retry connect in smartconfig");
        }
        else if(smartconfig_start_flag == 2)
        {
            if(smartconfig_retry_counter++ < 2)
            {
                led_network_status_handle(0);
                esp_wifi_stop();
                esp_wifi_set_mode(WIFI_MODE_STA);
                esp_wifi_start();
                esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
                esp_wifi_connect();
                ESP_LOGI(TAG, "retry to connect to the router in smartconfig in %d",smartconfig_retry_counter);
            }
            else
            {
                ESP_LOGI(TAG, "restart smartconfig");
                smartconfig_start();
            }
        }
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        led_network_status_handle(3);
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        tcp_event_send(TCP_EVENT_WIFI_CONNECTED);
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) 
    {
        ESP_LOGI(TAG, "Scan done");
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) 
    {
        ESP_LOGI(TAG, "Found channel");
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) 
    {
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;

        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        uint8_t rvd_data[33] = { 0 };
        extern uint8_t Local_AES_Key[32];

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }
        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        storage_save_key_blob("wifi_ssid",ssid,sizeof(evt->ssid));
        storage_save_key_blob("wifi_pwd",password,sizeof(evt->password));
        storage_save_key_value("wifi_mode",1);
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data));
            storage_save_key_blob("app_key",rvd_data,32);
            memcpy(Local_AES_Key,rvd_data,32);
            ESP_LOG_BUFFER_HEXDUMP("Local_AES_Key", rvd_data, 32, ESP_LOG_INFO);
        }

        smartconfig_start_flag = 2;
        smartconfig_wait_timer_stop();
        esp_wifi_disconnect();
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_wifi_connect();
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        ESP_LOGI(TAG, "smartconfig done");
        xSemaphoreGive( smartconfig_sem );
        esp_smartconfig_stop();
        smartconfig_start_flag = 0;
    }
}

void wifi_interface_init(void)
{
    smartconfig_sem = xSemaphoreCreateBinary();
    smartconfig_wait_timer_init();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_t instance_sc_event;
    ESP_ERROR_CHECK( esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK( esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    ESP_ERROR_CHECK( esp_event_handler_instance_register(SC_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_sc_event));

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}
