/**
 * @file mqtt_service.h
 * @brief MQTT 服务接口 (从 network_service 分离)
 */

#ifndef MQTT_SERVICE_H_
#define MQTT_SERVICE_H_

#include <stdint.h>
#include <stdbool.h>

/* ============================================================ */
/* MQTT 配置结构                                                   */
/* ============================================================ */

#define MQTT_URI_MAX_LEN   80
#define MQTT_USER_MAX_LEN  32
#define MQTT_PASS_MAX_LEN  32
#define MQTT_TOPIC_MAX_LEN 64

typedef struct
{
    char     uri[MQTT_URI_MAX_LEN];
    uint16_t port;
    char     username[MQTT_USER_MAX_LEN];
    char     password[MQTT_PASS_MAX_LEN];
    char     topic[MQTT_TOPIC_MAX_LEN];
} mqtt_config_t;

/* ============================================================ */
/* MQTT 状态                                                      */
/* ============================================================ */

typedef enum
{
    MQTT_STATUS_DISABLED          = 0x00,
    MQTT_STATUS_DNS_FAIL          = 0x01,
    MQTT_STATUS_TCP_CONNECT_FAIL  = 0x02,
    MQTT_STATUS_AUTH_FAIL         = 0x03,
    MQTT_STATUS_CONNECTING        = 0x04,
    MQTT_STATUS_CONNECTED         = 0x05,
    MQTT_STATUS_DISCONNECTED      = 0x06,
} mqtt_status_t;

/* ============================================================ */
/* 最近上报报文缓存                                                */
/* ============================================================ */

#define MQTT_RECENT_MAX      5
#define MQTT_RECENT_MSG_MAX  256   /* Rev.6: 从 512 改为 256 */

typedef struct
{
    uint32_t timestamp;                   /* Rev.6: Unix 秒时间戳 */
    uint16_t len;
    char     json[MQTT_RECENT_MSG_MAX];
} mqtt_recent_msg_t;

/* ============================================================ */
/* MQTT 接口                                                      */
/* ============================================================ */

void mqtt_service_init(void);

int  mqtt_service_start(void);
void mqtt_service_stop(void);
void mqtt_service_restart(void);
void mqtt_service_restart_async(void);

bool mqtt_service_is_connected(void);
bool mqtt_service_is_running(void);
int  mqtt_service_publish(const char *topic, const char *payload, int qos);

const mqtt_config_t *mqtt_service_config_get(void);
int                  mqtt_service_config_set(const mqtt_config_t *cfg);

uint32_t mqtt_service_report_interval_get(void);
void     mqtt_service_report_interval_set(uint32_t seconds);
uint32_t mqtt_service_last_report_time_get(void);

uint8_t  mqtt_service_recent_get(mqtt_recent_msg_t *out, uint8_t max_count);

mqtt_status_t mqtt_service_status_get(void);
bool          mqtt_service_enabled_get(void);
void          mqtt_service_enabled_set(bool enabled);

#endif /* MQTT_SERVICE_H_ */