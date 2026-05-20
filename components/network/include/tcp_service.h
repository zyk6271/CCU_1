/**
 * @file tcp_service.h
 * @brief TCP 服务接口 - 自定义TCP / TG服务器
 *
 * 双模式:
 *   自定义TCP: 不需要 app_key, 加密可选, 无心跳
 *   TG服务器:  需要 app_key, 强制 AES 加密, 连接后自动请求 Remote Key, 每 30s 心跳
 *
 * 生命周期:
 *   enabled  = 用户开关 (BLE CMD 0x02 / NVS), stop/start 不改它
 *   running  = task 控制 (start 设 true, stop 设 false)
 *   restart  = stop + start, 不影响 enabled
 */

#ifndef TCP_SERVICE_H_
#define TCP_SERVICE_H_

#include <stdint.h>
#include <stdbool.h>

/* ============================================================ */
/* 服务器模式                                                      */
/* ============================================================ */

typedef enum
{
    TCP_SERVER_MODE_CUSTOM   = 0x00,
    TCP_SERVER_MODE_TG       = 0x01,
} tcp_server_mode_t;

/* ============================================================ */
/* 配置                                                           */
/* ============================================================ */

#define TCP_HOST_MAX_LEN       80

#define DEF_CUSTOM_TCP_HOST    "tgc-tcp-uat.capax-tech.com"
#define DEF_CUSTOM_TCP_PORT    9000

#define DEF_TG_HOST            "smartdevice.towngas.com"
#define DEF_TG_PORT            10020

typedef struct
{
    char     host[TCP_HOST_MAX_LEN];
    uint16_t port;
    uint8_t  server_mode;       /* tcp_server_mode_t */
    uint8_t  encrypt_enabled;   /* 0=明文, 1=AES (TG模式强制1) */
} tcp_config_t;

/* ============================================================ */
/* 连接状态                                                       */
/* ============================================================ */

typedef enum
{
    TCP_STATUS_DISABLED       = 0x00,
    TCP_STATUS_DNS_FAIL       = 0x01,
    TCP_STATUS_CONNECT_FAIL   = 0x02,
    TCP_STATUS_CONNECTING     = 0x03,
    TCP_STATUS_CONNECTED      = 0x04,
    TCP_STATUS_DISCONNECTED   = 0x05,
    TCP_STATUS_WAITING_WIFI   = 0x06,
    TCP_STATUS_NO_KEY         = 0x07,
    TCP_STATUS_KEY_EXCHANGING = 0x08,
} tcp_status_t;

/* ============================================================ */
/* 配置状态 (BLE CMD 0x05 D12)                                    */
/* ============================================================ */

typedef enum
{
    TCP_CONFIG_NOT_CONFIGURED = 0x00,
    TCP_CONFIG_KEY_ONLY       = 0x01,
    TCP_CONFIG_FULL           = 0x02,
} tcp_config_status_t;

/* ============================================================ */
/* 常量                                                           */
/* ============================================================ */

#define TG_HEARTBEAT_INTERVAL_SEC   30

/* ============================================================ */
/* 连接成功回调                                                    */
/* ============================================================ */

/**
 * @brief 注册 TCP 连接成功回调
 *
 * TCP 会话建立后 (Custom 模式 socket 连接成功 / TG 模式密钥交换完成)
 * 调用此回调. 典型用途: 触发 Modbus 立即上报.
 *
 * @param cb  回调函数, NULL 取消注册
 */
typedef void (*tcp_on_connected_cb_t)(void);
void tcp_service_set_on_connected(tcp_on_connected_cb_t cb);

/* ============================================================ */
/* 服务控制                                                       */
/* ============================================================ */

void tcp_service_init(void);
int  tcp_service_start(void);
void tcp_service_stop(void);
void tcp_service_restart_async(void);

/* ============================================================ */
/* 运行时查询                                                      */
/* ============================================================ */

bool             tcp_service_is_connected(void);
bool             tcp_service_is_running(void);
tcp_status_t     tcp_service_status_get(void);
tcp_config_status_t tcp_service_config_status_get(void);

/* ============================================================ */
/* 数据发送                                                       */
/* ============================================================ */

int tcp_service_send(const uint8_t *data, size_t len);

/* ============================================================ */
/* 配置读写                                                       */
/* ============================================================ */

const tcp_config_t *tcp_service_config_get(void);
int                 tcp_service_config_set(const tcp_config_t *cfg);

uint32_t tcp_service_report_interval_get(void);
void     tcp_service_report_interval_set(uint32_t seconds);

/* ============================================================ */
/* 使能控制                                                       */
/* ============================================================ */

bool    tcp_service_enabled_get(void);
void    tcp_service_enabled_set(bool enabled);

/* ============================================================ */
/* TG 模式专用                                                    */
/* ============================================================ */

bool    tcp_service_key_is_valid(void);
uint8_t tcp_service_server_mode_get(void);
uint8_t tcp_service_encrypt_enabled_get(void);

/**
 * @brief 通知 TCP 服务: TG 密钥交换完成 (Remote Key/IV 已解析)
 *        由 system.c 的 wifi_data_handle() 处理 0x96 后调用
 */
void tcp_service_tg_key_exchange_done(void);

#endif /* TCP_SERVICE_H_ */
