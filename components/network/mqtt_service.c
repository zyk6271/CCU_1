/**
 * @file mqtt_service.c
 * @brief MQTT 服务实现 (从 network_service 分离)
 *
 * MQTT 上报 JSON 格式:
 *   {
 *     "Device ID": "AABBCCDDEEFF",
 *     "Instantflow": "7.809",
 *     "Totalflow": "3975.115"
 *   }
 *
 * 生命周期:
 *   s_mqtt_enabled: 用户层面的开关 (BLE CMD 0x01 / NVS)
 *   s_mqtt_running: task 层面的运行控制 (stop 设 false, task 检测后退出)
 *   restart = stop + start, 不影响 s_mqtt_enabled
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "mqtt_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "storage.h"
#include "mqtt_service.h"
#include "network_service.h"

#define TAG "mqtt_svc"

#define NVS_KEY_MQTT_CFG      "mqtt_cfg"
#define NVS_KEY_MQTT_RPT_INTV "mqtt_rpt"

#define DEF_MQTT_URI   "mqtt://3.105.103.105"
#define DEF_MQTT_USER  "towngas"
#define DEF_MQTT_PASS  "tg8425!"
#define DEF_MQTT_TOPIC "fairwood/towngas/gas"
#define DEF_REPORT_INTERVAL_SEC  (3 * 60)

#define REPORT_INTERVAL_MIN  10
#define REPORT_INTERVAL_MAX  360

#define WIFI_STATUS_CONNECTED_VAL  0x02

#define MQTT_TASK_STOP_TIMEOUT_MS  10000

/* ============================================================ */
/* 运行状态                                                       */
/* ============================================================ */

static esp_mqtt_client_handle_t s_mqtt_client  = NULL;
static volatile bool            s_mqtt_enabled = false;   /* 用户开关 */
static volatile bool            s_mqtt_running = false;   /* task 运行控制 */
static volatile mqtt_status_t   s_mqtt_status  = MQTT_STATUS_DISABLED;
static TaskHandle_t             s_mqtt_task    = NULL;

static volatile bool s_mqtt_restarting = false;   /* restart 重入锁 */

static mqtt_config_t s_mqtt_cfg;
static bool          s_mqtt_cfg_loaded = false;

static uint32_t          s_report_interval  = DEF_REPORT_INTERVAL_SEC;
static volatile uint32_t s_last_report_time = 0;

static mqtt_recent_msg_t s_recent[MQTT_RECENT_MAX];
static uint8_t s_recent_head  = 0;
static uint8_t s_recent_count = 0;

/* ============================================================ */
/* 最近上报缓存                                                    */
/* ============================================================ */

static void mqtt_recent_push(const char *json, uint16_t len)
{
    if (len > MQTT_RECENT_MSG_MAX)
    {
        len = MQTT_RECENT_MSG_MAX;
    }

    mqtt_recent_msg_t *slot = &s_recent[s_recent_head];
    time_t now;
    time(&now);
    slot->timestamp = (uint32_t)now;   /* Rev.6: 记录时间戳 */
    slot->len = len;
    memcpy(slot->json, json, len);
    s_recent_head = (s_recent_head + 1) % MQTT_RECENT_MAX;

    if (s_recent_count < MQTT_RECENT_MAX)
    {
        s_recent_count++;
    }
}

uint8_t mqtt_service_recent_get(mqtt_recent_msg_t *out, uint8_t max_count)
{
    uint8_t count = (s_recent_count < max_count) ? s_recent_count : max_count;

    for (uint8_t i = 0; i < count; i++)
    {
        uint8_t idx = (s_recent_head + MQTT_RECENT_MAX - 1 - i) % MQTT_RECENT_MAX;
        out[i] = s_recent[idx];
    }
    return count;
}

/* ============================================================ */
/* NVS 配置                                                       */
/* ============================================================ */

static void mqtt_config_load(void)
{
    uint32_t len = sizeof(mqtt_config_t);

    if (storage_read_key_blob(NVS_KEY_MQTT_CFG, (uint8_t *)&s_mqtt_cfg, &len) != ESP_OK ||
        len != sizeof(mqtt_config_t))
    {
        memset(&s_mqtt_cfg, 0, sizeof(s_mqtt_cfg));
        strncpy(s_mqtt_cfg.uri, DEF_MQTT_URI, sizeof(s_mqtt_cfg.uri) - 1);
        strncpy(s_mqtt_cfg.username, DEF_MQTT_USER, sizeof(s_mqtt_cfg.username) - 1);
        strncpy(s_mqtt_cfg.password, DEF_MQTT_PASS, sizeof(s_mqtt_cfg.password) - 1);
        strncpy(s_mqtt_cfg.topic, DEF_MQTT_TOPIC, sizeof(s_mqtt_cfg.topic) - 1);
        s_mqtt_cfg.port = 1883;
        ESP_LOGI(TAG, "MQTT cfg: defaults");
    }

    if (s_mqtt_cfg.port == 0)
    {
        s_mqtt_cfg.port = 1883;
        ESP_LOGW(TAG, "MQTT port was 0, reset to 1883");
    }

    s_mqtt_cfg_loaded = true;
}

static void mqtt_config_save(void)
{
    storage_save_key_blob(NVS_KEY_MQTT_CFG, (uint8_t *)&s_mqtt_cfg, sizeof(mqtt_config_t));
}

static void report_interval_load(void)
{
    uint32_t val = 0;
    if (storage_read_key_value(NVS_KEY_MQTT_RPT_INTV, &val) == ESP_OK &&
        val >= REPORT_INTERVAL_MIN)
    {
        s_report_interval = val;
    }
    ESP_LOGI(TAG, "MQTT report interval %lu s", (unsigned long)s_report_interval);
}

/* ============================================================ */
/* MQTT 事件处理                                                   */
/* ============================================================ */

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = data;

    switch ((esp_mqtt_event_id_t)id)
    {
    case MQTT_EVENT_CONNECTED:
        s_mqtt_status = MQTT_STATUS_CONNECTED;
        ESP_LOGI(TAG, "MQTT connected");
        break;

    case MQTT_EVENT_DISCONNECTED:
        if (s_mqtt_running)
        {
            s_mqtt_status = MQTT_STATUS_DISCONNECTED;
        }
        ESP_LOGW(TAG, "MQTT disconnected");
        break;

    case MQTT_EVENT_ERROR:
        if (event->error_handle)
        {
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
            {
                if (event->error_handle->esp_transport_sock_errno == ENOTCONN ||
                    event->error_handle->esp_transport_sock_errno == ECONNREFUSED)
                {
                    s_mqtt_status = MQTT_STATUS_TCP_CONNECT_FAIL;
                }
                else
                {
                    s_mqtt_status = MQTT_STATUS_DNS_FAIL;
                }
                ESP_LOGE(TAG, "MQTT transport error, errno=%d",
                         event->error_handle->esp_transport_sock_errno);
            }
            else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
            {
                s_mqtt_status = MQTT_STATUS_AUTH_FAIL;
                ESP_LOGE(TAG, "MQTT auth fail, code=%d",
                         event->error_handle->connect_return_code);
            }
            else
            {
                s_mqtt_status = MQTT_STATUS_TCP_CONNECT_FAIL;
            }
        }
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT pub id=%d", event->msg_id);
        break;

    default:
        break;
    }
}

/* ============================================================ */
/* MQTT 数据发布                                                   */
/* ============================================================ */

static void mqtt_publish_report(void)
{
    if (s_mqtt_status != MQTT_STATUS_CONNECTED || !s_mqtt_client)
    {
        return;
    }

    extern uint8_t modbus_gas_flow_sensor_value[32];

    uint32_t instant_raw = ((uint32_t)modbus_gas_flow_sensor_value[2] << 24) |
                           ((uint32_t)modbus_gas_flow_sensor_value[3] << 16) |
                           ((uint32_t)modbus_gas_flow_sensor_value[4] << 8)  |
                           modbus_gas_flow_sensor_value[5];
    float instant_flow = (float)instant_raw / 1000.0f;

    uint32_t total_v1 = ((uint32_t)modbus_gas_flow_sensor_value[6] << 24)  |
                        ((uint32_t)modbus_gas_flow_sensor_value[7] << 16)  |
                        ((uint32_t)modbus_gas_flow_sensor_value[8] << 8)   |
                        modbus_gas_flow_sensor_value[9];
    uint16_t total_v2 = ((uint16_t)modbus_gas_flow_sensor_value[10] << 8) |
                        modbus_gas_flow_sensor_value[11];
    float total_flow = (float)total_v1 + (float)total_v2 / 1000.0f;

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char device_id[13];
    snprintf(device_id, sizeof(device_id), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"Device ID\":\"%s\","
             "\"Instantflow\":\"%.3f\","
             "\"Totalflow\":\"%.3f\"}",
             device_id,
             instant_flow,
             total_flow);

    const char *topic = s_mqtt_cfg.topic[0] ? s_mqtt_cfg.topic : DEF_MQTT_TOPIC;
    time_t now;
    time(&now);

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 1, 0);

    if (msg_id >= 0)
    {
        s_last_report_time = (uint32_t)now;
        mqtt_recent_push(payload, (uint16_t)strlen(payload));
        ESP_LOGI(TAG, "MQTT pub: %s", payload);
    }
}

/* ============================================================ */
/* MQTT 任务                                                       */
/* ============================================================ */

static void mqtt_task_entry(void *param)
{
    char uri[128];

    extern uint8_t wifi_get_detail_status(void);
    ESP_LOGI(TAG, "MQTT waiting for WiFi...");

    while (s_mqtt_running && wifi_get_detail_status() != WIFI_STATUS_CONNECTED_VAL)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (!s_mqtt_running)
    {
        s_mqtt_status = MQTT_STATUS_DISABLED;
        s_mqtt_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "MQTT WiFi ready, starting connection");

    bool uri_has_port = false;
    const char *scheme_end = strstr(s_mqtt_cfg.uri, "://");
    if (scheme_end && strchr(scheme_end + 3, ':') != NULL)
    {
        uri_has_port = true;
    }

    if (s_mqtt_cfg.port > 0 && !uri_has_port)
    {
        snprintf(uri, sizeof(uri), "%s:%u", s_mqtt_cfg.uri, s_mqtt_cfg.port);
    }
    else
    {
        strncpy(uri, s_mqtt_cfg.uri, sizeof(uri) - 1);
        uri[sizeof(uri) - 1] = '\0';
    }

    ESP_LOGI(TAG, "MQTT uri=%s user=%s", uri, s_mqtt_cfg.username);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .credentials.username = s_mqtt_cfg.username,
        .credentials.authentication.password = s_mqtt_cfg.password,
        .session.keepalive = 30,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client)
    {
        s_mqtt_status = MQTT_STATUS_DISABLED;
        s_mqtt_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    s_mqtt_status = MQTT_STATUS_CONNECTING;
    esp_mqtt_client_start(s_mqtt_client);

    uint32_t elapsed = s_report_interval;
    while (s_mqtt_running)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        if (!s_mqtt_running)
        {
            break;
        }

        elapsed++;
        if (elapsed >= s_report_interval)
        {
            elapsed = 0;
            if (s_mqtt_status == MQTT_STATUS_CONNECTED)
            {
                mqtt_publish_report();
            }
        }
    }

    if (s_mqtt_client)
    {
        esp_mqtt_client_stop(s_mqtt_client);
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
    }

    s_mqtt_status = MQTT_STATUS_DISABLED;
    s_mqtt_task = NULL;
    vTaskDelete(NULL);
}

/* ============================================================ */
/* MQTT 控制接口                                                   */
/* ============================================================ */

int mqtt_service_start(void)
{
    if (s_mqtt_task)
    {
        return 0;
    }
    if (!s_mqtt_cfg_loaded)
    {
        mqtt_config_load();
    }

    s_mqtt_running = true;
    xTaskCreatePinnedToCore(mqtt_task_entry, "mqtt_svc", 4096, NULL, 3,
                            &s_mqtt_task, tskNO_AFFINITY);
    return 0;
}

void mqtt_service_stop(void)
{
    s_mqtt_running = false;

    if (s_mqtt_task)
    {
        xTaskNotifyGive(s_mqtt_task);
    }

    for (int i = 0; i < (MQTT_TASK_STOP_TIMEOUT_MS / 100) && s_mqtt_task != NULL; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (s_mqtt_task != NULL)
    {
        ESP_LOGW(TAG, "MQTT task stop timeout, force clear");
        if (s_mqtt_client) {
            esp_mqtt_client_stop(s_mqtt_client);
            esp_mqtt_client_destroy(s_mqtt_client);
            s_mqtt_client = NULL;
        }
        s_mqtt_task = NULL;
    }
}

void mqtt_service_restart(void)
{
    mqtt_service_stop();
    mqtt_config_load();
    mqtt_service_start();
}

static void mqtt_async_restart_entry(void *param)
{
    mqtt_service_restart();
    s_mqtt_restarting = false;
    vTaskDelete(NULL);
}

void mqtt_service_restart_async(void)
{
    if (s_mqtt_restarting)
    {
        ESP_LOGW(TAG, "MQTT restart already in progress, skip");
        return;
    }
    s_mqtt_restarting = true;
    xTaskCreate(mqtt_async_restart_entry, "mqtt_rst", 3072, NULL, 3, NULL);
}

bool mqtt_service_is_connected(void)
{
    return (s_mqtt_status == MQTT_STATUS_CONNECTED);
}

bool mqtt_service_is_running(void)
{
    return (s_mqtt_task != NULL);
}

int mqtt_service_publish(const char *topic, const char *payload, int qos)
{
    if (s_mqtt_status != MQTT_STATUS_CONNECTED || !s_mqtt_client)
    {
        return -1;
    }
    return esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, qos, 0);
}

/* ============================================================ */
/* MQTT 配置接口                                                   */
/* ============================================================ */

const mqtt_config_t *mqtt_service_config_get(void)
{
    if (!s_mqtt_cfg_loaded)
    {
        mqtt_config_load();
    }
    return &s_mqtt_cfg;
}

int mqtt_service_config_set(const mqtt_config_t *cfg)
{
    memcpy(&s_mqtt_cfg, cfg, sizeof(s_mqtt_cfg));
    s_mqtt_cfg.uri[MQTT_URI_MAX_LEN - 1] = '\0';
    s_mqtt_cfg.username[MQTT_USER_MAX_LEN - 1] = '\0';
    s_mqtt_cfg.password[MQTT_PASS_MAX_LEN - 1] = '\0';
    s_mqtt_cfg.topic[MQTT_TOPIC_MAX_LEN - 1] = '\0';
    if (s_mqtt_cfg.port == 0) s_mqtt_cfg.port = 1883;
    mqtt_config_save();
    return 0;
}

uint32_t mqtt_service_report_interval_get(void)
{
    return s_report_interval;
}

void mqtt_service_report_interval_set(uint32_t sec)
{
    if (sec < REPORT_INTERVAL_MIN)
    {
        sec = REPORT_INTERVAL_MIN;
    }
    if (sec > REPORT_INTERVAL_MAX)
    {
        sec = REPORT_INTERVAL_MAX;
    }
    s_report_interval = sec;
    storage_save_key_value(NVS_KEY_MQTT_RPT_INTV, sec);
}

uint32_t mqtt_service_last_report_time_get(void)
{
    return s_last_report_time;
}

mqtt_status_t mqtt_service_status_get(void)
{
    return s_mqtt_status;
}

bool mqtt_service_enabled_get(void)
{
    return s_mqtt_enabled;
}

void mqtt_service_enabled_set(bool enabled)
{
    s_mqtt_enabled = enabled;
}

/* ============================================================ */
/* 初始化                                                         */
/* ============================================================ */

void mqtt_service_init(void)
{
    mqtt_config_load();
    report_interval_load();
}