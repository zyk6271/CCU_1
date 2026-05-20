/**
 * @file wifi_manager.c
 * @brief WiFi 管理 (BLE NUS 配网 + SmartConfig + 详细状态)
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
#include "esp_smartconfig.h"
#include "storage.h"
#include "led.h"
#include "network_service.h"
#include "mqtt_service.h"
#include "tcp_service.h"
#include "wifi_manager.h"

#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "wifi_mgr";

/* ============================================================ */
/* 状态                                                          */
/* ============================================================ */

static wifi_status_t s_wifi_detail_status = WIFI_STATUS_IDLE;
static wifi_config_t s_saved_config;
static esp_timer_handle_t s_sc_timeout_timer;

uint8_t smartconfig_start_flag = 0;
static uint8_t s_sc_retry_count = 0;
static uint8_t s_ble_retry_count = 0;

#define BLE_CONFIG_MAX_RETRY  5

/* ============================================================ */
/* 内部工具                                                       */
/* ============================================================ */

static void update_led(void)
{
    switch (s_wifi_detail_status)
    {
    case WIFI_STATUS_IDLE:
    case WIFI_STATUS_PASSWORD_ERROR:
    case WIFI_STATUS_AP_NOT_FOUND:
    case WIFI_STATUS_AUTH_FAIL:
    case WIFI_STATUS_CONNECT_TIMEOUT:
        led_network_status_handle(0);
        break;
    case WIFI_STATUS_CONNECTING:
    case WIFI_STATUS_BLE_CONFIGURING:
        led_network_status_handle(2);
        break;
    case WIFI_STATUS_CONNECTED:
        led_network_status_handle(3);
        break;
    case WIFI_STATUS_SMARTCONFIG:
        led_network_status_handle(1);
        break;
    }
}

static void set_status(wifi_status_t new_status)
{
    ESP_LOGI(TAG, "WiFi status: %d -> %d", s_wifi_detail_status, new_status);
    s_wifi_detail_status = new_status;
    update_led();
}

static wifi_status_t map_disconnect_reason(uint8_t reason)
{
    switch (reason)
    {
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_NOT_AUTHED:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
        return WIFI_STATUS_PASSWORD_ERROR;

    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        return WIFI_STATUS_AP_NOT_FOUND;

    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_CONNECTION_FAIL:
        return WIFI_STATUS_AUTH_FAIL;

    case WIFI_REASON_ASSOC_TOOMANY:
    case WIFI_REASON_BEACON_TIMEOUT:
        return WIFI_STATUS_CONNECT_TIMEOUT;

    default:
        return WIFI_STATUS_IDLE;
    }
}

/* ============================================================ */
/* NVS                                                           */
/* ============================================================ */

static bool load_wifi_config_from_nvs(void)
{
    uint32_t flag = 0;
    if (storage_read_key_value("wifi_mode", &flag) != ESP_OK || !flag)
    {
        return false;
    }

    uint8_t ssid[33] = {0};
    uint32_t ssid_len = 0;
    uint8_t pass[65] = {0};
    uint32_t pass_len = 0;

    if (storage_read_key_blob("wifi_ssid", ssid, &ssid_len) != ESP_OK ||
        ssid_len == 0 || ssid_len > 32)
    {
        return false;
    }

    storage_read_key_blob("wifi_pwd", pass, &pass_len);

    memset(&s_saved_config, 0, sizeof(s_saved_config));
    memcpy(s_saved_config.sta.ssid, ssid, ssid_len);
    if (pass_len > 0 && pass_len <= 64)
    {
        memcpy(s_saved_config.sta.password, pass, pass_len);
    }

    ESP_LOGI(TAG, "Loaded SSID: %s", ssid);
    return true;
}

static void save_wifi_config_to_nvs(const char *ssid, const char *password)
{
    storage_save_key_blob("wifi_ssid", (uint8_t *)ssid, strlen(ssid));
    storage_save_key_blob("wifi_pwd", (uint8_t *)password, password ? strlen(password) : 0);
    storage_save_key_value("wifi_mode", 1);
}

/* ============================================================ */
/* SmartConfig                                                    */
/* ============================================================ */

static void sc_timeout_callback(void *arg)
{
    ESP_LOGW(TAG, "SmartConfig timeout");
    smartconfig_stop();
}

void smartconfig_start(void)
{
    if (smartconfig_start_flag != 0)
    {
        ESP_LOGW(TAG, "SmartConfig already running");
        return;
    }

    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    smartconfig_start_flag = 1;
    s_sc_retry_count = 0;
    set_status(WIFI_STATUS_SMARTCONFIG);

    esp_wifi_disconnect();
    esp_smartconfig_stop();
    esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2);
    esp_smartconfig_start(&cfg);

    esp_timer_stop(s_sc_timeout_timer);
    esp_timer_start_once(s_sc_timeout_timer, 3 * 60 * 1000 * 1000);

    ESP_LOGI(TAG, "SmartConfig started");
}

void smartconfig_stop(void)
{
    esp_timer_stop(s_sc_timeout_timer);
    smartconfig_start_flag = 0;
    esp_smartconfig_stop();

    set_status(WIFI_STATUS_CONNECTING);
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &s_saved_config);
    esp_wifi_connect();
}

/* ============================================================ */
/* BLE 配网                                                       */
/* ============================================================ */

int wifi_config_from_ble(const char *ssid, const char *password)
{
    if (!ssid || strlen(ssid) == 0 || strlen(ssid) > 32)
    {
        return -1;
    }
    if (password && strlen(password) > 64)
    {
        return -1;
    }

    if (smartconfig_start_flag != 0)
    {
        esp_timer_stop(s_sc_timeout_timer);
        esp_smartconfig_stop();
        smartconfig_start_flag = 0;
    }

    save_wifi_config_to_nvs(ssid, password);

    memset(&s_saved_config, 0, sizeof(s_saved_config));
    memcpy(s_saved_config.sta.ssid, ssid, strlen(ssid));
    if (password && strlen(password) > 0)
    {
        memcpy(s_saved_config.sta.password, password, strlen(password));
    }

    s_ble_retry_count = 0;
    set_status(WIFI_STATUS_BLE_CONFIGURING);

    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &s_saved_config);
    esp_wifi_connect();

    ESP_LOGI(TAG, "BLE config: SSID=%s", ssid);
    return 0;
}

/* ============================================================ */
/* 状态查询 API                                                   */
/* ============================================================ */

uint8_t wifi_get_detail_status(void)
{
    return (uint8_t)s_wifi_detail_status;
}

int8_t wifi_get_rssi(void)
{
    if (s_wifi_detail_status != WIFI_STATUS_CONNECTED)
    {
        return 0;
    }

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    {
        return ap_info.rssi;
    }
    return 0;
}

/* ============================================================ */
/* WiFi 事件处理                                                  */
/* ============================================================ */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        if (load_wifi_config_from_nvs())
        {
            set_status(WIFI_STATUS_CONNECTING);
            esp_wifi_set_config(WIFI_IF_STA, &s_saved_config);
            esp_wifi_connect();
        }
        else
        {
            set_status(WIFI_STATUS_IDLE);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "WiFi disconnected, reason=%d", event->reason);

        if (s_wifi_detail_status == WIFI_STATUS_BLE_CONFIGURING)
        {
            if (s_ble_retry_count < BLE_CONFIG_MAX_RETRY)
            {
                s_ble_retry_count++;
                esp_wifi_set_config(WIFI_IF_STA, &s_saved_config);
                esp_wifi_connect();
            }
            else
            {
                set_status(map_disconnect_reason(event->reason));
            }
        }
        else if (smartconfig_start_flag == 0)
        {
            set_status(WIFI_STATUS_CONNECTING);
            esp_wifi_set_config(WIFI_IF_STA, &s_saved_config);
            esp_wifi_connect();
        }
        else if (smartconfig_start_flag == 1)
        {
            /* SmartConfig 扫描中, 不重连 */
        }
        else if (smartconfig_start_flag == 2)
        {
            if (s_sc_retry_count < 5)
            {
                s_sc_retry_count++;
                esp_wifi_set_config(WIFI_IF_STA, &s_saved_config);
                esp_wifi_connect();
            }
            else
            {
                smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
                smartconfig_start_flag = 1;
                s_sc_retry_count = 0;
                set_status(WIFI_STATUS_SMARTCONFIG);
                esp_smartconfig_stop();
                esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2);
                esp_smartconfig_start(&cfg);
            }
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_sc_retry_count = 0;
        s_ble_retry_count = 0;
        smartconfig_start_flag = 0;
        set_status(WIFI_STATUS_CONNECTED);
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        /* Rev.6: WiFi 连接成功后, 重试启动未运行的已使能服务 */
        if (mqtt_service_enabled_get() && !mqtt_service_is_running())
        {
            ESP_LOGI(TAG, "MQTT enabled but not running, starting...");
            mqtt_service_start();
        }
        if (tcp_service_enabled_get() && !tcp_service_is_running())
        {
            ESP_LOGI(TAG, "TCP enabled but not running, starting...");
            tcp_service_start();
        }
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
    {
        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        extern uint8_t Local_AES_Key[32];

        memset(&s_saved_config, 0, sizeof(s_saved_config));
        memcpy(s_saved_config.sta.ssid, evt->ssid, sizeof(s_saved_config.sta.ssid));
        memcpy(s_saved_config.sta.password, evt->password, sizeof(s_saved_config.sta.password));
        s_saved_config.sta.bssid_set = evt->bssid_set;
        if (evt->bssid_set)
        {
            memcpy(s_saved_config.sta.bssid, evt->bssid, sizeof(s_saved_config.sta.bssid));
        }

        save_wifi_config_to_nvs((char *)evt->ssid, (char *)evt->password);

        if (evt->type == SC_TYPE_ESPTOUCH_V2)
        {
            uint8_t rvd[33] = {0};
            esp_smartconfig_get_rvd_data(rvd, sizeof(rvd));
            if (storage_save_key_blob("app_key", rvd, 32) == ESP_OK)
            {
                memcpy(Local_AES_Key, rvd, 32);
            }
        }

        smartconfig_start_flag = 2;
        set_status(WIFI_STATUS_CONNECTING);
        esp_wifi_disconnect();
        esp_wifi_set_config(ESP_IF_WIFI_STA, &s_saved_config);
        esp_wifi_connect();
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
    {
        esp_smartconfig_stop();
        esp_timer_stop(s_sc_timeout_timer);
        smartconfig_start_flag = 0;
    }
}

/* ============================================================ */
/* 初始化                                                         */
/* ============================================================ */

void wifi_interface_init(void)
{
    const esp_timer_create_args_t timer_args = {
        .callback = &sc_timeout_callback,
        .name = "sc_timeout"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_sc_timeout_timer));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t h1, h2, h3;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &h1));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &h2));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(SC_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &h3));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    set_status(WIFI_STATUS_IDLE);
    ESP_LOGI(TAG, "WiFi initialized");
}
