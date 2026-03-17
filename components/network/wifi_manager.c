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

#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "wifi station";

uint8_t smartconfig_start_flag = 0;
static uint8_t smartconfig_retry_counter = 0;

static wifi_config_t wifi_config;

static esp_timer_handle_t smartconfig_wait_timer;

// WiFi状态枚举
typedef enum {
    WIFI_STATE_IDLE = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_SMARTCONFIG,
    WIFI_STATE_SMARTCONFIG_GOT_CREDENTIALS,
    WIFI_STATE_ERROR
} wifi_state_t;

static wifi_state_t current_wifi_state = WIFI_STATE_IDLE;

static void set_wifi_state(wifi_state_t new_state) {
    wifi_state_t old_state = current_wifi_state;
    current_wifi_state = new_state;
    ESP_LOGI(TAG, "WiFi state changed: %d -> %d", old_state, new_state);
    switch(current_wifi_state)
    {
        case WIFI_STATE_IDLE:
            led_network_status_handle(0);
            break;
        case WIFI_STATE_CONNECTING:
            led_network_status_handle(2);
            break;
        case WIFI_STATE_SMARTCONFIG_GOT_CREDENTIALS:
            led_network_status_handle(2);
            break;
        case WIFI_STATE_CONNECTED:
            led_network_status_handle(3);
            break;
        case WIFI_STATE_SMARTCONFIG:
            led_network_status_handle(1);
            break;
        default:
            break;
    }
}

void smartconfig_wait_timer_callback(void* arg)
{
    ESP_LOGW(TAG, "SmartConfig timeout, stopping...");
    wifi_config_process_stop();
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

void wifi_config_process_stop(void)
{
    smartconfig_start_flag = 0;
    set_wifi_state(WIFI_STATE_IDLE);
    esp_smartconfig_stop();
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (strlen((char *)wifi_config.sta.ssid) > 0) 
    {
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_wifi_connect();
        ESP_LOGI(TAG, "wifi_config_process_stop: Try to connect saved AP");
    } 
    else 
    {
        ESP_LOGW(TAG, "wifi_config_process_stop: No valid config, stay idle");
    }
}

void wifi_config_process_restart(void)
{
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    smartconfig_start_flag = 1;
    smartconfig_retry_counter = 0;
    set_wifi_state(WIFI_STATE_SMARTCONFIG);
    esp_smartconfig_stop();
    esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2);
    esp_smartconfig_start(&cfg);
    ESP_LOGI(TAG, "wifi_config_process_restart");
}

void wifi_config_process_start(void)
{
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    if(smartconfig_start_flag != 0)
    {
        ESP_LOGW(TAG, "wifi_config_process is already start");
        return;
    }

    smartconfig_start_flag = 1;
    smartconfig_retry_counter = 0;
    set_wifi_state(WIFI_STATE_SMARTCONFIG);
    esp_smartconfig_stop();
    esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2);
    esp_smartconfig_start(&cfg);
    smartconfig_wait_timer_start();
    ESP_LOGI(TAG, "wifi_config_process_start");
}

static bool load_wifi_config_from_storage(void)
{
    uint32_t config_flag;
    if(storage_read_key_value("wifi_mode", &config_flag) != ESP_OK || !config_flag) {
        ESP_LOGI(TAG, "No saved WiFi config found");
        return false;
    }

    uint8_t ssid_temp[33] = {0};
    uint32_t ssid_length = 0;
    uint8_t password_temp[65] = {0};
    uint32_t password_length = 0;

    // 读取SSID
    if(storage_read_key_blob("wifi_ssid", ssid_temp, &ssid_length) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WiFi SSID from storage");
        return false;
    }

    // 读取密码
    if(storage_read_key_blob("wifi_pwd", password_temp, &password_length) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WiFi password from storage");
        return false;
    }

    // 边界检查
    if(ssid_length == 0 || ssid_length > 32) {
        ESP_LOGE(TAG, "Invalid SSID length: %lu", ssid_length);
        return false;
    }

    if(password_length > 64) {
        ESP_LOGE(TAG, "Invalid password length: %lu", password_length);
        return false;
    }

    ESP_LOGI(TAG, "Loaded SSID: %s (length: %lu)", ssid_temp, ssid_length);
    ESP_LOGI(TAG, "Loaded PASSWORD: [%lu chars]", password_length);

    bzero(&wifi_config, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, ssid_temp, ssid_length);
    if(password_length > 0) {
        memcpy(wifi_config.sta.password, password_temp, password_length);
    }

    return true;
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started");
        
        if(load_wifi_config_from_storage()) {
            set_wifi_state(WIFI_STATE_CONNECTING);
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_connect();
            ESP_LOGI(TAG, "Attempting to connect to saved WiFi");
        } else {
            set_wifi_state(WIFI_STATE_IDLE);
            ESP_LOGI(TAG, "No saved WiFi config, waiting for SmartConfig");
        }
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGI(TAG, "WiFi disconnected, reason: %d", event->reason);
        
        if(smartconfig_start_flag == 0) {
            set_wifi_state(WIFI_STATE_CONNECTING);
            // esp_wifi_stop();
            // esp_wifi_set_mode(WIFI_MODE_STA);
            // esp_wifi_start();
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_connect();
            ESP_LOGI(TAG, "Retry connect to AP");
        } 
        else if(smartconfig_start_flag == 1) {
            ESP_LOGI(TAG, "Not allow retry connect in smartconfig scanning");
        } 
        else if(smartconfig_start_flag == 2) {
            // SmartConfig已获取凭证，正在尝试连接
            if(smartconfig_retry_counter < 5) {
                smartconfig_retry_counter++;
                set_wifi_state(WIFI_STATE_SMARTCONFIG_GOT_CREDENTIALS);
                // esp_wifi_stop();
                // esp_wifi_set_mode(WIFI_MODE_STA);
                // esp_wifi_start();
                esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
                esp_wifi_connect();
                ESP_LOGI(TAG, "Retry connect to router in smartconfig, attempt %d", smartconfig_retry_counter);
            } 
            else 
            {
                ESP_LOGW(TAG, "SmartConfig connect failed, restarting smartconfig");
                wifi_config_process_restart();
            }
        }
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        smartconfig_retry_counter = 0;
        smartconfig_start_flag = 0;
        
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        set_wifi_state(WIFI_STATE_CONNECTED);
        tcp_event_send(TCP_EVENT_WIFI_CONNECTED);
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) 
    {
        ESP_LOGI(TAG, "SmartConfig scan done");
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) 
    {
        ESP_LOGI(TAG, "SmartConfig found channel");
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) 
    {
        ESP_LOGI(TAG, "SmartConfig got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;

        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        uint8_t rvd_data[33] = { 0 };
        extern uint8_t Local_AES_Key[32];

        memset(&wifi_config, 0, sizeof(wifi_config));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

        // 保存到存储
        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        
        esp_err_t save_ret;
        save_ret = storage_save_key_blob("wifi_ssid", ssid, sizeof(evt->ssid));
        if (save_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save SSID to storage: %s", esp_err_to_name(save_ret));
        }
        
        save_ret = storage_save_key_blob("wifi_pwd", password, sizeof(evt->password));
        if (save_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save password to storage: %s", esp_err_to_name(save_ret));
        }
        
        save_ret = storage_save_key_value("wifi_mode", 1);
        if (save_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save wifi_mode to storage: %s", esp_err_to_name(save_ret));
        }
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data));
            save_ret = storage_save_key_blob("app_key", rvd_data, 32);
            if (save_ret == ESP_OK) {
                memcpy(Local_AES_Key, rvd_data, 32);
                ESP_LOG_BUFFER_HEXDUMP("Local_AES_Key", rvd_data, 32, ESP_LOG_INFO);
            } else {
                ESP_LOGE(TAG, "Failed to save app_key to storage: %s", esp_err_to_name(save_ret));
            }
        }

        smartconfig_start_flag = 2;
        set_wifi_state(WIFI_STATE_SMARTCONFIG_GOT_CREDENTIALS);
        esp_wifi_disconnect();
        esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
        esp_wifi_connect();
        ESP_LOGI(TAG, "Attempting to connect with SmartConfig credentials");
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        ESP_LOGI(TAG, "SmartConfig ACK sent done");
        smartconfig_wait_timer_stop();
        smartconfig_start_flag = 0;
        set_wifi_state(WIFI_STATE_CONNECTED);
    }
}

void wifi_interface_init(void)
{
    const esp_timer_create_args_t timer_args = 
    {
        .callback = &smartconfig_wait_timer_callback,
        .name = "smartconfig_wait_timer"
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &smartconfig_wait_timer));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_t instance_sc_event;
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(SC_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_sc_event));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    set_wifi_state(WIFI_STATE_IDLE);
    ESP_LOGI(TAG, "WiFi interface initialized");
}