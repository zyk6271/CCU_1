/**
 * @file network_service.c
 * @brief 网络服务管理器 - 统一入口 (MQTT + TCP + SNTP + 状态查询)
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_system.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "storage.h"
#include "iot_event.h"
#include "network_service.h"
#include "esp_sntp.h"

#define TAG "net_svc"

#define NVS_KEY_SVC_FLAGS  "svc_flags"
#define NVS_KEY_SNTP_EN    "sntp_en"

/* ============================================================ */
/* SNTP 运行状态                                                   */
/* ============================================================ */

static volatile uint8_t s_sntp_dev_sync_en = 1;
static volatile uint8_t s_sntp_sync_status = SNTP_SYNC_NONE;

/* ============================================================ */
/* 服务使能标志                                                    */
/* ============================================================ */

static void service_flags_load(void)
{
    uint32_t flags = 0;
    bool mqtt_en = false;
    bool tcp_en  = true;

    if (storage_read_key_value(NVS_KEY_SVC_FLAGS, &flags) == ESP_OK)
    {
        mqtt_en = (flags & 1) != 0;
        tcp_en  = (flags & 2) != 0;
    }

    mqtt_service_enabled_set(mqtt_en);
    tcp_service_enabled_set(tcp_en);

    ESP_LOGI(TAG, "Flags MQTT=%d TCP=%d", mqtt_en, tcp_en);
}

int network_service_save_enable_flags(uint8_t mqtt_en, uint8_t tcp_en)
{
    uint32_t flags = (mqtt_en ? 1 : 0) | (tcp_en ? 2 : 0);
    return (storage_save_key_value(NVS_KEY_SVC_FLAGS, flags) == ESP_OK) ? 0 : -1;
}

/* ============================================================ */
/* SNTP 时间同步                                                   */
/* ============================================================ */

static void sntp_sync_callback(struct timeval *tv)
{
    s_sntp_sync_status = SNTP_SYNC_COMPLETED;
    ESP_LOGI(TAG, "SNTP time synchronized");
}

static void sntp_flag_load(void)
{
    uint32_t val = 1;
    storage_read_key_value(NVS_KEY_SNTP_EN, &val);
    s_sntp_dev_sync_en = (uint8_t)(val & 0xFF);
}

static void sntp_service_start(void)
{
    if (esp_sntp_enabled())
    {
        return;
    }

    ESP_LOGI(TAG, "SNTP starting");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.windows.com");
    esp_sntp_setservername(2, "ntp.aliyun.com");
    sntp_set_time_sync_notification_cb(sntp_sync_callback);
    s_sntp_sync_status = SNTP_SYNC_IN_PROGRESS;
    esp_sntp_init();
    setenv("TZ", "HKT-8", 1);
    tzset();
}

void network_sntp_enable_set(uint8_t enable)
{
    s_sntp_dev_sync_en = enable ? 1 : 0;
    storage_save_key_value(NVS_KEY_SNTP_EN, s_sntp_dev_sync_en);
    ESP_LOGI(TAG, "SNTP device sync %s", s_sntp_dev_sync_en ? "enabled" : "disabled");
}

uint8_t network_sntp_enable_get(void)
{
    return s_sntp_dev_sync_en;
}

uint8_t network_sntp_sync_status_get(void)
{
    return s_sntp_sync_status;
}

/* ============================================================ */
/* 状态查询                                                       */
/* ============================================================ */

all_connection_status_t network_all_status_get(void)
{
    all_connection_status_t status;
    memset(&status, 0, sizeof(status));

    extern uint8_t wifi_get_detail_status(void);
    extern int8_t wifi_get_rssi(void);
    extern uint8_t modbus_detect_result_read(void);

    status.wifi_status         = wifi_get_detail_status();
    status.wifi_rssi           = wifi_get_rssi();
    status.mqtt_enabled        = mqtt_service_enabled_get() ? 1 : 0;
    status.mqtt_status         = (uint8_t)mqtt_service_status_get();
    status.tcp_enabled         = tcp_service_enabled_get() ? 1 : 0;
    status.tcp_status          = (uint8_t)tcp_service_status_get();
    status.modbus_status       = modbus_detect_result_read();
    status.mqtt_last_report_time = mqtt_service_last_report_time_get();
    status.tcp_key_configured  = tcp_service_key_is_valid() ? 1 : 0;
    status.tcp_config_status   = (uint8_t)tcp_service_config_status_get();
    status.sntp_enabled        = s_sntp_dev_sync_en;
    status.sntp_sync_status    = s_sntp_sync_status;
    status.tcp_server_mode     = tcp_service_server_mode_get();
    status.tcp_encrypt_enabled = tcp_service_encrypt_enabled_get();

    return status;
}

/* ============================================================ */
/* 恢复出厂设置                                                    */
/* ============================================================ */

void network_factory_reset_prepare(void)
{
    ESP_LOGW(TAG, "Factory reset: stopping services and erasing NVS");
    mqtt_service_stop();
    tcp_service_stop();
    storage_erase_all();
}

void network_factory_reset(void)
{
    network_factory_reset_prepare();
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

/* ============================================================ */
/* 初始化                                                         */
/* ============================================================ */

void network_service_init(void)
{
    /* EventGroup (仅供 TCP_CONNECT_RESET 使用) */
    tcp_event_init();

    /* 子服务初始化 (加载 NVS 配置) */
    mqtt_service_init();
    tcp_service_init();

    /* 注册 TCP 连接成功回调: 触发 Modbus 立即上报 */
    extern void wifi_ccu_modbus_poll_upload(void);
    tcp_service_set_on_connected(wifi_ccu_modbus_poll_upload);

    /* 加载使能标志 */
    service_flags_load();

    /* SNTP */
    sntp_flag_load();
    sntp_service_start();

    /* 启动已使能的服务 */
    if (mqtt_service_enabled_get())
    {
        mqtt_service_start();
    }
    if (tcp_service_enabled_get())
    {
        tcp_service_start();
    }

    ESP_LOGI(TAG, "Network init OK (tcp_mode=%s, tg_key=%s, encrypt=%s, sntp=%s)",
             tcp_service_server_mode_get() == TCP_SERVER_MODE_TG ? "TG" : "Custom",
             tcp_service_key_is_valid() ? "valid" : "missing",
             tcp_service_encrypt_enabled_get() ? "on" : "off",
             s_sntp_dev_sync_en ? "on" : "off");
}
