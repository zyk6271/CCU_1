/**
 * @file tcp_service.c
 * @brief TCP 服务实现 - 自定义TCP / TG服务器
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "esp_system.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "storage.h"
#include "iot_event.h"
#include "wifi_service.h"
#include "tcp_service.h"
#include "crypto_aes.h"
#include "system.h"

#define TAG "tcp_svc"

/* ============================================================ */
/* NVS 键名                                                       */
/* ============================================================ */

#define NVS_KEY_TCP_CFG       "tcp_cfg"
#define NVS_KEY_TCP_RPT_INTV  "tcp_rpt"
#define NVS_KEY_TCP_APP_KEY   "app_key"

/* ============================================================ */
/* 常量                                                           */
/* ============================================================ */

#define TCP_RECV_BUFSZ            1024
#define REPORT_INTERVAL_MIN       10
#define REPORT_INTERVAL_MAX       360
#define DEF_REPORT_INTERVAL_SEC   (3 * 60)
#define TG_CTRL_KEY_REQUEST       0xFF
#define TCP_TASK_STOP_TIMEOUT_MS  10000
#define RECONNECT_DELAY_STEPS     30        /* 30 × 100ms = 3s */
#define TCP_CONNECT_TIMEOUT_SEC   10

#define WIFI_CONNECTED_STATUS     0x02      /* wifi_manager 的 WIFI_STATUS_CONNECTED */

/* ============================================================ */
/* 运行状态                                                       */
/* ============================================================ */

static volatile bool         s_tcp_enabled  = false;
static volatile bool         s_tcp_running  = false;
static volatile tcp_status_t s_tcp_status   = TCP_STATUS_DISABLED;
static TaskHandle_t          s_tcp_task     = NULL;
static int                   s_tcp_sock     = -1;

static volatile bool s_tcp_restarting = false;

static tcp_config_t s_tcp_cfg;
static bool         s_tcp_cfg_loaded = false;

static uint32_t s_report_interval  = DEF_REPORT_INTERVAL_SEC;
static bool     s_tcp_key_valid    = false;

static volatile bool s_tg_remote_key_ready = false;

/* ============================================================ */
/* 连接成功回调                                                    */
/* ============================================================ */

static tcp_on_connected_cb_t s_on_connected_cb = NULL;

void tcp_service_set_on_connected(tcp_on_connected_cb_t cb)
{
    s_on_connected_cb = cb;
}

static void notify_connected(void)
{
    if (s_on_connected_cb)
    {
        s_on_connected_cb();
    }
}

/* ============================================================ */
/* 可打断的重连等待                                                 */
/* ============================================================ */

static void interruptible_delay_3s(void)
{
    for (int i = 0; i < RECONNECT_DELAY_STEPS && s_tcp_running; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ============================================================ */
/* TG 密钥检查                                                    */
/* ============================================================ */

static bool tcp_app_key_exists(void)
{
    uint8_t key_buf[32] = {0};
    uint32_t key_len = sizeof(key_buf);

    if (storage_read_key_blob(NVS_KEY_TCP_APP_KEY, key_buf, &key_len) != ESP_OK ||
        key_len != 32)
    {
        ESP_LOGW(TAG, "TG app_key not found");
        return false;
    }

    bool all_zero = true;
    for (int i = 0; i < 32; i++)
    {
        if (key_buf[i] != 0)
        {
            all_zero = false;
            break;
        }
    }

    if (all_zero)
    {
        ESP_LOGW(TAG, "TG app_key is all zeros, invalid");
        return false;
    }

    ESP_LOGI(TAG, "TG app_key valid");
    return true;
}

/* ============================================================ */
/* NVS 配置                                                       */
/* ============================================================ */

static void tcp_config_load(void)
{
    uint32_t len = sizeof(tcp_config_t);

    if (storage_read_key_blob(NVS_KEY_TCP_CFG, (uint8_t *)&s_tcp_cfg, &len) != ESP_OK ||
        len != sizeof(tcp_config_t))
    {
        memset(&s_tcp_cfg, 0, sizeof(s_tcp_cfg));
        strncpy(s_tcp_cfg.host, DEF_CUSTOM_TCP_HOST, sizeof(s_tcp_cfg.host) - 1);
        s_tcp_cfg.port = DEF_CUSTOM_TCP_PORT;
        s_tcp_cfg.server_mode = TCP_SERVER_MODE_CUSTOM;
        s_tcp_cfg.encrypt_enabled = 0;
        ESP_LOGI(TAG, "TCP cfg: defaults (Custom, no encrypt)");
    }

    /* TG 模式强制加密 */
    if (s_tcp_cfg.server_mode == TCP_SERVER_MODE_TG)
    {
        s_tcp_cfg.encrypt_enabled = 1;
    }

    /* 无效模式回退 */
    if (s_tcp_cfg.server_mode != TCP_SERVER_MODE_CUSTOM &&
        s_tcp_cfg.server_mode != TCP_SERVER_MODE_TG)
    {
        s_tcp_cfg.server_mode = TCP_SERVER_MODE_CUSTOM;
        s_tcp_cfg.encrypt_enabled = 0;
    }

    s_tcp_cfg_loaded = true;

    ESP_LOGI(TAG, "TCP cfg: %s:%d mode=%s encrypt=%s",
             s_tcp_cfg.host, s_tcp_cfg.port,
             s_tcp_cfg.server_mode == TCP_SERVER_MODE_TG ? "TG" : "Custom",
             s_tcp_cfg.encrypt_enabled ? "on" : "off");
}

static void tcp_config_save(void)
{
    storage_save_key_blob(NVS_KEY_TCP_CFG, (uint8_t *)&s_tcp_cfg, sizeof(tcp_config_t));
}

static void report_interval_load(void)
{
    uint32_t val = 0;
    if (storage_read_key_value(NVS_KEY_TCP_RPT_INTV, &val) == ESP_OK &&
        val >= REPORT_INTERVAL_MIN)
    {
        s_report_interval = val;
    }
    ESP_LOGI(TAG, "TCP report interval %lu s", (unsigned long)s_report_interval);
}

/* ============================================================ */
/* TG 密钥请求与心跳                                               */
/* ============================================================ */

static void tg_send_key_request(void)
{
    uint8_t plain[2] = {0x00, 0x01};
    uint8_t *enc_ptr = NULL;
    uint32_t enc_size = 0;

    crypto_aes_local_encrypt(plain, 2, &enc_ptr, &enc_size);
    uint16_t slen = set_wifi_uart_buffer(0, enc_ptr, enc_size);
    wifi_uart_write_frame(TG_CTRL_KEY_REQUEST, 2, slen);
    free(enc_ptr);

    ESP_LOGI(TAG, "TG key request sent");
}

static void tg_send_heartbeat(void)
{
    extern uint8_t tcp_send_count_read(void);

    uint8_t plain[2];
    plain[0] = tcp_send_count_read();
    plain[1] = 0x01;

    uint8_t *enc_ptr = NULL;
    uint32_t enc_size = 0;

    crypto_aes_remote_encrypt(plain, 2, &enc_ptr, &enc_size);
    uint16_t slen = set_wifi_uart_buffer(0, enc_ptr, enc_size);
    wifi_uart_write_frame(TG_CTRL_KEY_REQUEST, 2, slen);
    free(enc_ptr);
}

void tcp_service_tg_key_exchange_done(void)
{
    s_tg_remote_key_ready = true;
    s_tcp_status = TCP_STATUS_CONNECTED;
    notify_connected();
    ESP_LOGI(TAG, "TG Remote Key/IV received, session ready");
}

/* ============================================================ */
/* Socket 操作                                                    */
/* ============================================================ */

static void tcp_close_socket(void)
{
    if (s_tcp_sock >= 0)
    {
        close(s_tcp_sock);
        s_tcp_sock = -1;
    }
}

int tcp_service_send(const uint8_t *data, size_t len)
{
    if (s_tcp_sock < 0)
    {
        return -1;
    }
    if (send(s_tcp_sock, data, len, 0) < 0)
    {
        tcp_event_send(TCP_CONNECT_RESET);
        return -1;
    }
    return 0;
}

/**
 * @brief  非阻塞 connect, 带超时, 可被 s_tcp_running 打断
 * @return  0=连接成功, -1=连接失败或被打断
 */
static int tcp_nonblocking_connect(int sock, struct sockaddr_in *addr,
                                   const char *host, uint16_t port)
{
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0)
    {
        ESP_LOGE(TAG, "fcntl F_GETFL fail");
        return -1;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        ESP_LOGE(TAG, "fcntl O_NONBLOCK fail");
        return -1;
    }

    int ret = connect(sock, (struct sockaddr *)addr, sizeof(*addr));

    if (ret == 0)
    {
        fcntl(sock, F_SETFL, flags);
        return 0;
    }

    if (errno != EINPROGRESS)
    {
        ESP_LOGE(TAG, "TCP connect fail: %s:%d errno=%d", host, port, errno);
        fcntl(sock, F_SETFL, flags);
        return -1;
    }

    /* EINPROGRESS: 连接进行中, 每秒轮询, 检查 s_tcp_running */
    for (int t = 0; t < TCP_CONNECT_TIMEOUT_SEC; t++)
    {
        if (!s_tcp_running)
        {
            ESP_LOGI(TAG, "TCP connect aborted by stop");
            fcntl(sock, F_SETFL, flags);
            return -1;
        }

        fd_set wr_fds;
        FD_ZERO(&wr_fds);
        FD_SET(sock, &wr_fds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

        int sel = select(sock + 1, NULL, &wr_fds, NULL, &tv);

        if (sel > 0)
        {
            int so_err = 0;
            socklen_t so_len = sizeof(so_err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_err, &so_len);

            if (so_err == 0)
            {
                fcntl(sock, F_SETFL, flags);
                return 0;
            }

            ESP_LOGE(TAG, "TCP connect fail: %s:%d so_err=%d", host, port, so_err);
            fcntl(sock, F_SETFL, flags);
            return -1;
        }
        else if (sel < 0)
        {
            ESP_LOGE(TAG, "TCP connect select error: errno=%d", errno);
            fcntl(sock, F_SETFL, flags);
            return -1;
        }
    }

    ESP_LOGE(TAG, "TCP connect timeout: %s:%d (%ds)", host, port, TCP_CONNECT_TIMEOUT_SEC);
    fcntl(sock, F_SETFL, flags);
    return -1;
}

/* ============================================================ */
/* TCP 主任务                                                      */
/* ============================================================ */

static void tcp_task_entry(void *param)
{
    uint8_t *recv_buf = calloc(1, TCP_RECV_BUFSZ);
    if (!recv_buf)
    {
        s_tcp_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    fd_set read_fds;

    /* 等待 WiFi */
    extern uint8_t wifi_get_detail_status(void);
    s_tcp_status = TCP_STATUS_WAITING_WIFI;
    ESP_LOGI(TAG, "TCP waiting for WiFi...");

    while (s_tcp_running && wifi_get_detail_status() != WIFI_CONNECTED_STATUS)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (!s_tcp_running)
    {
        free(recv_buf);
        s_tcp_status = TCP_STATUS_DISABLED;
        s_tcp_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP WiFi ready");

    /* 主循环: 连接 → 收发 → 断开 → 重连 */
    while (s_tcp_running)
    {
        s_tcp_status = TCP_STATUS_CONNECTING;
        s_tg_remote_key_ready = false;

        const char *host = s_tcp_cfg.host;
        uint16_t port = s_tcp_cfg.port;

        /* DNS 解析 */
        struct hostent *host_entry = gethostbyname(host);
        if (!host_entry)
        {
            s_tcp_status = TCP_STATUS_DNS_FAIL;
            ESP_LOGE(TAG, "TCP DNS fail: %s", host);
            interruptible_delay_3s();
            continue;
        }

        struct sockaddr_in server_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr = *(struct in_addr *)host_entry->h_addr,
        };

        /* 创建 socket */
        s_tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (s_tcp_sock < 0)
        {
            interruptible_delay_3s();
            continue;
        }

        /* 非阻塞连接 */
        if (tcp_nonblocking_connect(s_tcp_sock, &server_addr, host, port) != 0)
        {
            s_tcp_status = TCP_STATUS_CONNECT_FAIL;
            tcp_close_socket();
            if (!s_tcp_running)
            {
                break;
            }
            interruptible_delay_3s();
            continue;
        }

        ESP_LOGI(TAG, "TCP connected %s:%d (mode=%s)",
                 host, port,
                 s_tcp_cfg.server_mode == TCP_SERVER_MODE_TG ? "TG" : "Custom");

        /* 模式分支: TG 需要密钥交换, Custom 直接就绪 */
        if (s_tcp_cfg.server_mode == TCP_SERVER_MODE_TG)
        {
            s_tcp_status = TCP_STATUS_KEY_EXCHANGING;
            tg_send_key_request();
        }
        else
        {
            s_tcp_status = TCP_STATUS_CONNECTED;
            notify_connected();
        }

        /* 接收循环 */
        uint32_t heartbeat_counter = 0;
        while (s_tcp_running && s_tcp_sock >= 0)
        {
            if (tcp_event_recv(TCP_CONNECT_RESET, 0) & TCP_CONNECT_RESET)
            {
                break;
            }

            FD_ZERO(&read_fds);
            FD_SET(s_tcp_sock, &read_fds);

            struct timeval sel_timeout = { .tv_sec = 1 };
            int sel_ret = select(s_tcp_sock + 1, &read_fds, NULL, NULL, &sel_timeout);

            if (sel_ret > 0)
            {
                int bytes_read = recv(s_tcp_sock, recv_buf, TCP_RECV_BUFSZ - 1, 0);
                if (bytes_read <= 0)
                {
                    break;
                }
                wifi_recv_buffer(recv_buf, bytes_read);
            }

            /* TG 心跳 */
            if (s_tcp_cfg.server_mode == TCP_SERVER_MODE_TG && s_tg_remote_key_ready)
            {
                heartbeat_counter++;
                if (heartbeat_counter >= TG_HEARTBEAT_INTERVAL_SEC)
                {
                    heartbeat_counter = 0;
                    tg_send_heartbeat();
                }
            }
        }

        tcp_close_socket();
        s_tcp_status = TCP_STATUS_DISCONNECTED;
        s_tg_remote_key_ready = false;
        interruptible_delay_3s();
    }

    free(recv_buf);
    s_tcp_status = TCP_STATUS_DISABLED;
    s_tcp_task = NULL;
    vTaskDelete(NULL);
}

/* ============================================================ */
/* 服务控制                                                       */
/* ============================================================ */

int tcp_service_start(void)
{
    if (s_tcp_task)
    {
        return 0;
    }
    if (!s_tcp_cfg_loaded)
    {
        tcp_config_load();
    }

    if (s_tcp_cfg.server_mode == TCP_SERVER_MODE_TG && !s_tcp_key_valid)
    {
        ESP_LOGW(TAG, "TG start blocked: no app_key");
        s_tcp_status = TCP_STATUS_NO_KEY;
        return -1;
    }

    s_tcp_running = true;
    xTaskCreatePinnedToCore(tcp_task_entry, "tcp_svc", 4096, NULL, 3,
                            &s_tcp_task, tskNO_AFFINITY);
    return 0;
}

void tcp_service_stop(void)
{
    s_tcp_running = false;
    tcp_close_socket();

    for (int i = 0; i < (TCP_TASK_STOP_TIMEOUT_MS / 100) && s_tcp_task != NULL; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (s_tcp_task != NULL)
    {
        ESP_LOGW(TAG, "TCP task stop timeout, force clear");
        s_tcp_task = NULL;
    }
}

bool tcp_service_is_connected(void)
{
    return (s_tcp_status == TCP_STATUS_CONNECTED);
}

bool tcp_service_is_running(void)
{
    return (s_tcp_task != NULL);
}

static void tcp_async_restart_entry(void *param)
{
    tcp_service_stop();
    tcp_service_start();
    s_tcp_restarting = false;
    vTaskDelete(NULL);
}

void tcp_service_restart_async(void)
{
    if (s_tcp_restarting)
    {
        ESP_LOGW(TAG, "TCP restart already in progress, skip");
        return;
    }
    s_tcp_restarting = true;
    xTaskCreate(tcp_async_restart_entry, "tcp_rst", 3072, NULL, 3, NULL);
}

/* ============================================================ */
/* 配置读写                                                       */
/* ============================================================ */

const tcp_config_t *tcp_service_config_get(void)
{
    if (!s_tcp_cfg_loaded)
    {
        tcp_config_load();
    }
    return &s_tcp_cfg;
}

int tcp_service_config_set(const tcp_config_t *cfg)
{
    memcpy(&s_tcp_cfg, cfg, sizeof(s_tcp_cfg));
    s_tcp_cfg.host[TCP_HOST_MAX_LEN - 1] = '\0';

    if (s_tcp_cfg.server_mode == TCP_SERVER_MODE_TG)
    {
        s_tcp_cfg.encrypt_enabled = 1;
    }

    if (s_tcp_cfg.server_mode != TCP_SERVER_MODE_CUSTOM &&
        s_tcp_cfg.server_mode != TCP_SERVER_MODE_TG)
    {
        s_tcp_cfg.server_mode = TCP_SERVER_MODE_CUSTOM;
        s_tcp_cfg.encrypt_enabled = 0;
    }

    tcp_config_save();

    ESP_LOGI(TAG, "TCP config set: %s:%d mode=%s encrypt=%s",
             s_tcp_cfg.host, s_tcp_cfg.port,
             s_tcp_cfg.server_mode == TCP_SERVER_MODE_TG ? "TG" : "Custom",
             s_tcp_cfg.encrypt_enabled ? "on" : "off");
    return 0;
}

uint32_t tcp_service_report_interval_get(void)
{
    return s_report_interval;
}

void tcp_service_report_interval_set(uint32_t sec)
{
    if (sec < REPORT_INTERVAL_MIN) sec = REPORT_INTERVAL_MIN;
    if (sec > REPORT_INTERVAL_MAX) sec = REPORT_INTERVAL_MAX;
    s_report_interval = sec;
    storage_save_key_value(NVS_KEY_TCP_RPT_INTV, sec);
}

/* ============================================================ */
/* 状态查询                                                       */
/* ============================================================ */

bool tcp_service_key_is_valid(void)
{
    return s_tcp_key_valid;
}

uint8_t tcp_service_server_mode_get(void)
{
    return s_tcp_cfg.server_mode;
}

uint8_t tcp_service_encrypt_enabled_get(void)
{
    return s_tcp_cfg.encrypt_enabled;
}

tcp_status_t tcp_service_status_get(void)
{
    return s_tcp_status;
}

tcp_config_status_t tcp_service_config_status_get(void)
{
    if (s_tcp_cfg.server_mode == TCP_SERVER_MODE_CUSTOM)
    {
        return TCP_CONFIG_FULL;
    }

    if (!s_tcp_key_valid)
    {
        return TCP_CONFIG_NOT_CONFIGURED;
    }
    if (strcmp(s_tcp_cfg.host, DEF_TG_HOST) == 0 && s_tcp_cfg.port == DEF_TG_PORT)
    {
        return TCP_CONFIG_KEY_ONLY;
    }
    return TCP_CONFIG_FULL;
}

bool tcp_service_enabled_get(void)
{
    return s_tcp_enabled;
}

void tcp_service_enabled_set(bool enabled)
{
    s_tcp_enabled = enabled;
}

/* ============================================================ */
/* 初始化                                                         */
/* ============================================================ */

void tcp_service_init(void)
{
    tcp_config_load();
    report_interval_load();
    s_tcp_key_valid = tcp_app_key_exists();
}
