/**
 * @file ble_cmd_handler.c
 * @brief BLE 指令解析与分发 (Modbus 专用 V2.1 Rev.5)
 *
 * Rev.5 变更:
 *   - CMD 0x09 读取 TCP 配置: 应答 84B, 新增 server_mode + encrypt_enabled 字段
 *   - CMD 0x0A 写入 TCP 配置: 请求 84B, 新增 server_mode + encrypt_enabled 字段
 *   - CMD 0x05 读全部连接状态: 应答 17B, 新增 D15 tcp_server_mode, D16 tcp_encrypt_enabled
 */

#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_app_desc.h"
#include "esp_mac.h"
#include "esp_timer.h"

#include "ble_cmd_handler.h"
#include "ble_uart.h"
#include "network_service.h"
#include "mqtt_service.h"
#include "tcp_service.h"
#include "storage.h"
#include "wifi_manager.h"
#include "ccu_modbus_api.h"

#define TAG "ble_cmd"

#define RECV_BUF_SIZE  600
static uint8_t  recv_buf[RECV_BUF_SIZE];
static uint16_t recv_buf_len = 0;

/* ============================================================ */
/* 字节序工具                                                      */
/* ============================================================ */

static uint8_t calc_checksum(const uint8_t *data, uint16_t len)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        sum += data[i];
    }
    return sum;
}

static void encode_u16_le(uint8_t *buf, uint16_t val)
{
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
}

static void encode_u32_le(uint8_t *buf, uint32_t val)
{
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
}

static void encode_u32_be(uint8_t *buf, uint32_t val)
{
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

static uint16_t decode_u16_le(const uint8_t *buf)
{
    return buf[0] | (buf[1] << 8);
}

static uint32_t decode_u32_le(const uint8_t *buf)
{
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

static uint32_t decode_u32_be(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | buf[3];
}

/* ============================================================ */
/* 帧发送                                                         */
/* ============================================================ */

void ble_cmd_send_response(uint8_t cmd, const uint8_t *data, uint16_t data_len)
{
    uint16_t frame_len = 6 + data_len;
    uint8_t frame[BLE_UART_BUF_SIZE];

    if (frame_len > sizeof(frame))
    {
        return;
    }

    uint16_t pos = 0;
    frame[pos++] = BLE_FRAME_HEAD;
    frame[pos++] = cmd | 0x80;
    frame[pos++] = data_len & 0xFF;
    frame[pos++] = (data_len >> 8) & 0xFF;

    if (data_len && data)
    {
        memcpy(frame + pos, data, data_len);
        pos += data_len;
    }

    frame[pos] = calc_checksum(frame, pos);
    pos++;
    frame[pos++] = BLE_FRAME_TAIL;

    ble_uart_send(frame, pos);
}

static void ble_cmd_send_result_code(uint8_t cmd, uint8_t code)
{
    ble_cmd_send_response(cmd, &code, 1);
}

/* ============================================================ */
/* 延迟重启                                                        */
/* ============================================================ */

static void delayed_restart_callback(void *arg)
{
    ble_uart_disconnect();
    esp_restart();
}

static void ble_cmd_delayed_restart(void)
{
    esp_timer_handle_t timer = NULL;
    const esp_timer_create_args_t args = {
        .callback = delayed_restart_callback,
        .name = "rst_tmr",
    };
    if (esp_timer_create(&args, &timer) == ESP_OK)
    {
        esp_timer_start_once(timer, 1500 * 1000);
    }
}

/* ============================================================ */
/* 0x01 MQTT 开关                                                  */
/* ============================================================ */

static void handle_mqtt_switch(const uint8_t *data, uint16_t len)
{
    if (len < 1)
    {
        ble_cmd_send_result_code(BLE_CMD_MQTT_SWITCH, 0x0F);
        return;
    }

    mqtt_service_enabled_set(data[0] ? true : false);

    if (data[0])
    {
        mqtt_service_start();
    }
    else
    {
        mqtt_service_stop();
    }

    all_connection_status_t st = network_all_status_get();
    network_service_save_enable_flags(data[0] ? 1 : 0, st.tcp_enabled);
    ble_cmd_send_result_code(BLE_CMD_MQTT_SWITCH, 0x00);
}

/* ============================================================ */
/* 0x02 TCP 服务器开关                                             */
/* ============================================================ */

static void handle_tcp_switch(const uint8_t *data, uint16_t len)
{
    if (len < 1)
    {
        ble_cmd_send_result_code(BLE_CMD_TCP_SWITCH, 0x0F);
        return;
    }

    tcp_service_enabled_set(data[0] ? true : false);

    if (data[0])
    {
        tcp_service_start();
    }
    else
    {
        tcp_service_stop();
    }

    all_connection_status_t st = network_all_status_get();
    network_service_save_enable_flags(st.mqtt_enabled, data[0] ? 1 : 0);
    ble_cmd_send_result_code(BLE_CMD_TCP_SWITCH, 0x00);
}

/* ============================================================ */
/* 0x03 读 MQTT 配置                                               */
/* ============================================================ */

static void handle_read_mqtt_config(void)
{
    const mqtt_config_t *cfg = mqtt_service_config_get();
    uint8_t buf[MQTT_URI_MAX_LEN + 2 + MQTT_USER_MAX_LEN + MQTT_PASS_MAX_LEN + MQTT_TOPIC_MAX_LEN];
    memset(buf, 0, sizeof(buf));

    uint16_t pos = 0;
    memcpy(buf + pos, cfg->uri, strlen(cfg->uri));
    pos += MQTT_URI_MAX_LEN;

    encode_u16_le(buf + pos, cfg->port);
    pos += 2;

    memcpy(buf + pos, cfg->username, strlen(cfg->username));
    pos += MQTT_USER_MAX_LEN;

    memcpy(buf + pos, cfg->password, strlen(cfg->password));
    pos += MQTT_PASS_MAX_LEN;

    memcpy(buf + pos, cfg->topic, strlen(cfg->topic));
    pos += MQTT_TOPIC_MAX_LEN;

    ble_cmd_send_response(BLE_CMD_READ_MQTT_CONFIG, buf, pos);
}

/* ============================================================ */
/* 0x04 写 MQTT 配置                                               */
/* ============================================================ */

static void handle_write_mqtt_config(const uint8_t *data, uint16_t len)
{
    const uint16_t need = MQTT_URI_MAX_LEN + 2 + MQTT_USER_MAX_LEN +
                          MQTT_PASS_MAX_LEN + MQTT_TOPIC_MAX_LEN;

    if (len < need)
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MQTT_CONFIG, 0x0F);
        return;
    }

    mqtt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    uint16_t pos = 0;
    memcpy(cfg.uri, data + pos, MQTT_URI_MAX_LEN);
    cfg.uri[MQTT_URI_MAX_LEN - 1] = '\0';
    pos += MQTT_URI_MAX_LEN;

    cfg.port = decode_u16_le(data + pos);
    pos += 2;

    memcpy(cfg.username, data + pos, MQTT_USER_MAX_LEN);
    cfg.username[MQTT_USER_MAX_LEN - 1] = '\0';
    pos += MQTT_USER_MAX_LEN;

    memcpy(cfg.password, data + pos, MQTT_PASS_MAX_LEN);
    cfg.password[MQTT_PASS_MAX_LEN - 1] = '\0';
    pos += MQTT_PASS_MAX_LEN;

    memcpy(cfg.topic, data + pos, MQTT_TOPIC_MAX_LEN);
    cfg.topic[MQTT_TOPIC_MAX_LEN - 1] = '\0';

    if (cfg.uri[0] == '\0')
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MQTT_CONFIG, 0x0F);
        return;
    }
    if (cfg.port == 0)
    {
        cfg.port = 1883;
    }

    mqtt_service_config_set(&cfg);

    if (mqtt_service_enabled_get())
    {
        mqtt_service_restart_async();
    }

    ble_cmd_send_result_code(BLE_CMD_WRITE_MQTT_CONFIG, 0x00);
}

/* ============================================================ */
/* 0x05 读全部连接状态 (Rev.5: 17B)                                 */
/* ============================================================ */

static void handle_read_all_status(void)
{
    all_connection_status_t st = network_all_status_get();
    uint8_t buf[17];

    buf[0]  = st.wifi_status;
    buf[1]  = (uint8_t)st.wifi_rssi;
    buf[2]  = st.mqtt_enabled;
    buf[3]  = st.mqtt_status;
    buf[4]  = st.tcp_enabled;
    buf[5]  = st.tcp_status;
    buf[6]  = st.modbus_status;
    encode_u32_le(buf + 7, st.mqtt_last_report_time);
    buf[11] = st.tcp_key_configured;
    buf[12] = st.tcp_config_status;
    buf[13] = st.sntp_enabled;
    buf[14] = st.sntp_sync_status;
    buf[15] = st.tcp_server_mode;       /* Rev.5: 0=TG, 1=自定义 */
    buf[16] = st.tcp_encrypt_enabled;   /* Rev.5: 0=明文, 1=加密 */

    ble_cmd_send_response(BLE_CMD_READ_ALL_STATUS, buf, 17);
}

/* ============================================================ */
/* 0x06/0x07 MQTT 上报间隔                                         */
/* ============================================================ */

static void handle_read_mqtt_report_interval(void)
{
    uint8_t buf[4];
    encode_u32_le(buf, mqtt_service_report_interval_get());
    ble_cmd_send_response(BLE_CMD_READ_MQTT_REPORT_INTV, buf, 4);
}

static void handle_write_mqtt_report_interval(const uint8_t *data, uint16_t len)
{
    if (len < 4)
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MQTT_REPORT_INTV, 0x0F);
        return;
    }
    mqtt_service_report_interval_set(decode_u32_le(data));
    ble_cmd_send_result_code(BLE_CMD_WRITE_MQTT_REPORT_INTV, 0x00);
}

/* ============================================================ */
/* 0x08 读设备信息 (46B)                                            */
/* ============================================================ */

static void handle_read_device_info(void)
{
    extern uint8_t firmware_rev_val[];

    uint8_t buf[46];
    memset(buf, 0, sizeof(buf));

    esp_read_mac(buf, ESP_MAC_BT);
    strncpy((char *)(buf + 6), (const char *)firmware_rev_val, 32);

    uint32_t uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    encode_u32_le(buf + 38, uptime_sec);

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    uint32_t dev_time = (timeinfo.tm_year > (2020 - 1900)) ? (uint32_t)now : 0;
    encode_u32_le(buf + 42, dev_time);

    ble_cmd_send_response(BLE_CMD_READ_DEVICE_INFO, buf, 46);
}

/* ============================================================ */
/* 0x09 读取 TCP 服务器配置 (Rev.5: 84B)                            */
/*   Host(80B) + Port(2B) + server_mode(1B) + encrypt_enabled(1B) */
/* ============================================================ */

static void handle_read_tcp_config(void)
{
    const tcp_config_t *cfg = tcp_service_config_get();
    uint8_t buf[TCP_HOST_MAX_LEN + 2 + 2];
    memset(buf, 0, sizeof(buf));

    uint16_t pos = 0;
    memcpy(buf + pos, cfg->host, strlen(cfg->host));
    pos += TCP_HOST_MAX_LEN;

    encode_u16_le(buf + pos, cfg->port);
    pos += 2;

    buf[pos++] = cfg->server_mode;       /* server_mode: 0=自定义, 1=TG */
    buf[pos++] = cfg->encrypt_enabled;   /* encrypt_enabled: 0=明文, 1=加密 */

    ble_cmd_send_response(BLE_CMD_READ_TCP_CONFIG, buf, pos);
}

/* ============================================================ */
/* 0x0A 写入 TCP 服务器配置 (Rev.5: 82~84B)                        */
/*   Host(80B) + Port(2B) + [server_mode(1B) + encrypt_enabled(1B)] */
/*                                                                 */
/*   兼容性: 如果 APP 只发 82B (无 mode/encrypt 字段),               */
/*           则保持当前 server_mode 和 encrypt_enabled 不变           */
/* ============================================================ */

static void handle_write_tcp_config(const uint8_t *data, uint16_t len)
{
    const uint16_t need = TCP_HOST_MAX_LEN + 2;
    if (len < need)
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_TCP_CONFIG, 0x0F);
        return;
    }

    /* 读取当前配置作为默认值 (兼容旧 APP 不传 mode/encrypt) */
    const tcp_config_t *current = tcp_service_config_get();

    tcp_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    uint16_t pos = 0;
    memcpy(cfg.host, data + pos, TCP_HOST_MAX_LEN);
    cfg.host[TCP_HOST_MAX_LEN - 1] = '\0';
    pos += TCP_HOST_MAX_LEN;

    cfg.port = decode_u16_le(data + pos);
    pos += 2;

    if (len >= need + 2)
    {
        /* 新版 APP: 包含 server_mode + encrypt_enabled */
        cfg.server_mode = data[pos++];
        cfg.encrypt_enabled = data[pos++];
    }
    else
    {
        /* 旧版 APP: 保持当前设置不变 */
        cfg.server_mode = current->server_mode;
        cfg.encrypt_enabled = current->encrypt_enabled;
    }

    tcp_service_config_set(&cfg);

    /* 只要 TCP 服务是启用状态就重启, 无论当前是否在运行或已连接 */
    if (tcp_service_enabled_get())
    {
        tcp_service_restart_async();
    }

    ble_cmd_send_result_code(BLE_CMD_WRITE_TCP_CONFIG, 0x00);
}

/* ============================================================ */
/* 0x0B WiFi 配网                                                  */
/* ============================================================ */

static void handle_wifi_config(const uint8_t *data, uint16_t len)
{
    if (len < 2)
    {
        ble_cmd_send_result_code(BLE_CMD_WIFI_CONFIG, 0x0F);
        return;
    }

    uint8_t ssid_len = data[0];
    if (ssid_len == 0 || ssid_len > 32 || len < 1 + ssid_len + 1)
    {
        ble_cmd_send_result_code(BLE_CMD_WIFI_CONFIG, 0x0F);
        return;
    }

    uint8_t pass_len = data[1 + ssid_len];
    if (pass_len > 64 || len < 1 + ssid_len + 1 + pass_len)
    {
        ble_cmd_send_result_code(BLE_CMD_WIFI_CONFIG, 0x0F);
        return;
    }

    char ssid[33] = {0};
    char password[65] = {0};
    memcpy(ssid, data + 1, ssid_len);
    if (pass_len > 0)
    {
        memcpy(password, data + 1 + ssid_len + 1, pass_len);
    }

    int ret = wifi_config_from_ble(ssid, password);
    ble_cmd_send_result_code(BLE_CMD_WIFI_CONFIG, (ret == 0) ? 0x00 : 0x0F);
}

/* ============================================================ */
/* 0x0C SmartConfig / 0x0D 重启 / 0x0E 出厂设置                    */
/* ============================================================ */

static void handle_smartconfig_start(void)
{
    smartconfig_start();
    ble_cmd_send_result_code(BLE_CMD_SMARTCONFIG_START, 0x00);
}

static void handle_device_restart(void)
{
    ble_cmd_send_result_code(BLE_CMD_DEVICE_RESTART, 0x00);
    ble_cmd_delayed_restart();
}

static void handle_factory_reset(void)
{
    ble_cmd_send_result_code(BLE_CMD_FACTORY_RESET, 0x00);
    network_factory_reset_prepare();
    ble_cmd_delayed_restart();
}

/* ============================================================ */
/* 0x10/0x11 TCP 上报间隔                                          */
/* ============================================================ */

static void handle_read_tcp_report_interval(void)
{
    uint8_t buf[4];
    encode_u32_le(buf, tcp_service_report_interval_get());
    ble_cmd_send_response(BLE_CMD_READ_TCP_REPORT_INTV, buf, 4);
}

static void handle_write_tcp_report_interval(const uint8_t *data, uint16_t len)
{
    if (len < 4)
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_TCP_REPORT_INTV, 0x0F);
        return;
    }
    tcp_service_report_interval_set(decode_u32_le(data));
    ble_cmd_send_result_code(BLE_CMD_WRITE_TCP_REPORT_INTV, 0x00);
}

/* ============================================================ */
/* 0x20~0x24 Modbus 设备管理                                       */
/* ============================================================ */

static void handle_read_modbus_dev_status(void)
{
    ccu_modbus_device_status_t devs[MODBUS_DEVICE_TYPE_COUNT];
    ccu_modbus_device_status_get_all(devs);

    uint8_t buf[1 + MODBUS_DEVICE_TYPE_COUNT * 16];
    memset(buf, 0, sizeof(buf));
    buf[0] = MODBUS_DEVICE_TYPE_COUNT;

    for (int i = 0; i < MODBUS_DEVICE_TYPE_COUNT; i++)
    {
        uint8_t *p = buf + 1 + i * 16;
        const char *name = ccu_modbus_device_name_get(i);
        strncpy((char *)p, name, 8);
        p[8]  = ccu_modbus_device_addr_get(i);
        p[9]  = devs[i].enabled;
        p[10] = devs[i].status;
        p[11] = 0;
        encode_u16_le(p + 12, devs[i].reg_start);
        encode_u16_le(p + 14, devs[i].reg_count);
    }

    ble_cmd_send_response(BLE_CMD_READ_MODBUS_DEV_STATUS, buf,
                          1 + MODBUS_DEVICE_TYPE_COUNT * 16);
}

static void handle_write_modbus_dev_enable(const uint8_t *data, uint16_t len)
{
    if (len < 2)
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MODBUS_DEV_ENABLE, 0x0F);
        return;
    }

    if (ccu_modbus_device_enable_set(data[0], data[1]) != 0)
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MODBUS_DEV_ENABLE, 0x0F);
        return;
    }
    ble_cmd_send_result_code(BLE_CMD_WRITE_MODBUS_DEV_ENABLE, 0x00);
}

static void handle_read_modbus_log(void)
{
    ccu_modbus_log_entry_t logs[10];
    uint8_t count = ccu_modbus_log_read(logs, 10);

    uint8_t buf[1 + 10 * 32];
    memset(buf, 0, sizeof(buf));
    buf[0] = count;

    for (int i = 0; i < count; i++)
    {
        uint8_t *p = buf + 1 + i * 32;
        encode_u32_le(p, logs[i].timestamp);
        strncpy((char *)(p + 4), logs[i].device_name, 16);
        p[20] = logs[i].result;
        p[21] = logs[i].slave_addr;
        p[22] = logs[i].operation;
        p[23] = 0;
        encode_u16_le(p + 24, logs[i].reg_start);
        encode_u16_le(p + 26, logs[i].reg_count);
    }

    ble_cmd_send_response(BLE_CMD_READ_MODBUS_LOG, buf, 1 + count * 32);
}

static void handle_write_modbus_dev_addr(const uint8_t *data, uint16_t len)
{
    if (len < 5)
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MODBUS_DEV_ADDR, 0x0F);
        return;
    }

    int ret = ccu_modbus_device_reg_addr_set(data[0], decode_u16_le(data + 1),
                                              decode_u16_le(data + 3));
    if (ret == -1)
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MODBUS_DEV_ADDR, 0x0F);
    }
    else if (ret == -2)
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MODBUS_DEV_ADDR, 0x02);
    }
    else
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MODBUS_DEV_ADDR, 0x00);
    }
}

static void handle_write_modbus_slave_addr(const uint8_t *data, uint16_t len)
{
    if (len < 2)
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MODBUS_SLAVE_ADDR, 0x0F);
        return;
    }

    int ret = ccu_modbus_device_slave_addr_set(data[0], data[1]);
    if (ret == -1)
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MODBUS_SLAVE_ADDR, 0x0F);
    }
    else if (ret == -2)
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MODBUS_SLAVE_ADDR, 0x02);
    }
    else
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MODBUS_SLAVE_ADDR, 0x00);
    }
}

/* ============================================================ */
/* 0x25~0x2A SNTP / WiFi / Modbus 轮询 / MQTT 历史                */
/* ============================================================ */

static void handle_sntp_switch(const uint8_t *data, uint16_t len)
{
    if (len < 1)
    {
        ble_cmd_send_result_code(BLE_CMD_SNTP_SWITCH, 0x0F);
        return;
    }
    network_sntp_enable_set(data[0] ? 1 : 0);
    ble_cmd_send_result_code(BLE_CMD_SNTP_SWITCH, 0x00);
}

static void handle_read_sntp_status(void)
{
    uint8_t buf[2];
    buf[0] = network_sntp_enable_get();
    buf[1] = network_sntp_sync_status_get();
    ble_cmd_send_response(BLE_CMD_READ_SNTP_STATUS, buf, 2);
}

static void handle_read_wifi_config(void)
{
    uint8_t buf[97];
    memset(buf, 0, sizeof(buf));

    uint8_t ssid[33] = {0};
    uint32_t ssid_len = 32;
    if (storage_read_key_blob("wifi_ssid", ssid, &ssid_len) == ESP_OK &&
        ssid_len > 0 && ssid_len <= 32)
    {
        buf[0] = (uint8_t)ssid_len;
        memcpy(buf + 1, ssid, ssid_len);
    }

    uint8_t pass[65] = {0};
    uint32_t pass_len = 64;
    if (storage_read_key_blob("wifi_pwd", pass, &pass_len) == ESP_OK &&
        pass_len <= 64)
    {
        buf[33] = (uint8_t)pass_len;
        memcpy(buf + 34, pass, pass_len);
    }

    ble_cmd_send_response(BLE_CMD_READ_WIFI_CONFIG, buf, 97);
}

static void handle_read_modbus_poll_interval(void)
{
    uint8_t buf[4];
    encode_u32_le(buf, ccu_modbus_poll_interval_get());
    ble_cmd_send_response(BLE_CMD_READ_MODBUS_POLL_INTV, buf, 4);
}

static void handle_write_modbus_poll_interval(const uint8_t *data, uint16_t len)
{
    if (len < 4)
    {
        ble_cmd_send_result_code(BLE_CMD_WRITE_MODBUS_POLL_INTV, 0x0F);
        return;
    }

    int ret = ccu_modbus_poll_interval_set(decode_u32_le(data));
    ble_cmd_send_result_code(BLE_CMD_WRITE_MODBUS_POLL_INTV, (ret == 0) ? 0x00 : 0x02);
}

static void handle_read_mqtt_recent(void)
{
    static mqtt_recent_msg_t msgs[MQTT_RECENT_MAX];
    static uint8_t buf[1 + MQTT_RECENT_MAX * (4 + 2 + MQTT_RECENT_MSG_MAX)]; /* Rev.6: +4B timestamp */

    uint8_t count = mqtt_service_recent_get(msgs, MQTT_RECENT_MAX);

    uint16_t pos = 0;
    buf[pos++] = count;

    for (uint8_t i = 0; i < count; i++)
    {
        encode_u32_le(buf + pos, msgs[i].timestamp);  /* Rev.6: 时间戳 */
        pos += 4;
        encode_u16_le(buf + pos, msgs[i].len);
        pos += 2;
        memcpy(buf + pos, msgs[i].json, msgs[i].len);
        pos += msgs[i].len;
    }

    ble_cmd_send_response(BLE_CMD_READ_MQTT_RECENT, buf, pos);
}

/* ============================================================ */
/* OTA                                                             */
/* ============================================================ */

static struct
{
    bool active;
    esp_ota_handle_t handle;
    const esp_partition_t *partition;
    uint32_t firmware_size;
    uint32_t bytes_written;
} ota_session;

static void ota_session_abort(void)
{
    if (ota_session.active && ota_session.handle)
    {
        esp_ota_abort(ota_session.handle);
    }
    memset(&ota_session, 0, sizeof(ota_session));
}

static void handle_ota_start(const uint8_t *data, uint16_t len)
{
    if (len < 4)
    {
        ble_cmd_send_result_code(BLE_CMD_OTA_START, 0xFF);
        return;
    }

    ota_session_abort();
    ota_session.firmware_size = decode_u32_be(data);

    ota_session.partition = esp_ota_get_next_update_partition(NULL);
    if (!ota_session.partition)
    {
        ble_cmd_send_result_code(BLE_CMD_OTA_START, 0x01);
        return;
    }

    if (ota_session.firmware_size > ota_session.partition->size)
    {
        ble_cmd_send_result_code(BLE_CMD_OTA_START, 0x02);
        return;
    }

    if (esp_ota_begin(ota_session.partition, ota_session.firmware_size,
                      &ota_session.handle) != ESP_OK)
    {
        ble_cmd_send_result_code(BLE_CMD_OTA_START, 0x01);
        return;
    }

    ota_session.active = true;
    ota_session.bytes_written = 0;
    ble_cmd_send_result_code(BLE_CMD_OTA_START, 0x00);
}

static void handle_ota_write_data(const uint8_t *data, uint16_t len)
{
    if (!ota_session.active || len == 0)
    {
        ble_cmd_send_result_code(BLE_CMD_OTA_DATA, 0xFF);
        return;
    }
    if (esp_ota_write(ota_session.handle, data, len) != ESP_OK)
    {
        ota_session_abort();
        ble_cmd_send_result_code(BLE_CMD_OTA_DATA, 0x02);
        return;
    }

    ota_session.bytes_written += len;
    uint8_t resp[5];
    resp[0] = 0x00;
    encode_u32_be(resp + 1, ota_session.bytes_written);
    ble_cmd_send_response(BLE_CMD_OTA_DATA, resp, 5);
}

static void handle_ota_confirm_upgrade(void)
{
    if (!ota_session.active)
    {
        ble_cmd_send_result_code(BLE_CMD_OTA_EXECUTE, 0xFF);
        return;
    }

    if (ota_session.bytes_written < ota_session.firmware_size)
    {
        ble_cmd_send_result_code(BLE_CMD_OTA_EXECUTE, 0x03);
        return;
    }

    if (esp_ota_end(ota_session.handle) != ESP_OK)
    {
        ota_session_abort();
        ble_cmd_send_result_code(BLE_CMD_OTA_EXECUTE, 0x01);
        return;
    }

    ota_session.handle = 0;

    if (esp_ota_set_boot_partition(ota_session.partition) != ESP_OK)
    {
        ota_session_abort();
        ble_cmd_send_result_code(BLE_CMD_OTA_EXECUTE, 0x02);
        return;
    }

    ota_session.active = false;
    ble_cmd_send_result_code(BLE_CMD_OTA_EXECUTE, 0x00);
    ble_cmd_delayed_restart();
}

/* ============================================================ */
/* 指令分发                                                        */
/* ============================================================ */

static void dispatch_command(uint8_t cmd, const uint8_t *data, uint16_t data_len)
{
    switch (cmd)
    {
    case BLE_CMD_MQTT_SWITCH:
        handle_mqtt_switch(data, data_len);
        break;
    case BLE_CMD_TCP_SWITCH:
        handle_tcp_switch(data, data_len);
        break;
    case BLE_CMD_READ_MQTT_CONFIG:
        handle_read_mqtt_config();
        break;
    case BLE_CMD_WRITE_MQTT_CONFIG:
        handle_write_mqtt_config(data, data_len);
        break;
    case BLE_CMD_READ_ALL_STATUS:
        handle_read_all_status();
        break;
    case BLE_CMD_READ_MQTT_REPORT_INTV:
        handle_read_mqtt_report_interval();
        break;
    case BLE_CMD_WRITE_MQTT_REPORT_INTV:
        handle_write_mqtt_report_interval(data, data_len);
        break;
    case BLE_CMD_READ_DEVICE_INFO:
        handle_read_device_info();
        break;
    case BLE_CMD_READ_TCP_CONFIG:
        handle_read_tcp_config();
        break;
    case BLE_CMD_WRITE_TCP_CONFIG:
        handle_write_tcp_config(data, data_len);
        break;
    case BLE_CMD_WIFI_CONFIG:
        handle_wifi_config(data, data_len);
        break;
    case BLE_CMD_SMARTCONFIG_START:
        handle_smartconfig_start();
        break;
    case BLE_CMD_DEVICE_RESTART:
        handle_device_restart();
        break;
    case BLE_CMD_FACTORY_RESET:
        handle_factory_reset();
        break;
    case BLE_CMD_READ_TCP_REPORT_INTV:
        handle_read_tcp_report_interval();
        break;
    case BLE_CMD_WRITE_TCP_REPORT_INTV:
        handle_write_tcp_report_interval(data, data_len);
        break;
    case BLE_CMD_READ_MODBUS_DEV_STATUS:
        handle_read_modbus_dev_status();
        break;
    case BLE_CMD_WRITE_MODBUS_DEV_ENABLE:
        handle_write_modbus_dev_enable(data, data_len);
        break;
    case BLE_CMD_READ_MODBUS_LOG:
        handle_read_modbus_log();
        break;
    case BLE_CMD_WRITE_MODBUS_DEV_ADDR:
        handle_write_modbus_dev_addr(data, data_len);
        break;
    case BLE_CMD_WRITE_MODBUS_SLAVE_ADDR:
        handle_write_modbus_slave_addr(data, data_len);
        break;
    case BLE_CMD_SNTP_SWITCH:
        handle_sntp_switch(data, data_len);
        break;
    case BLE_CMD_READ_SNTP_STATUS:
        handle_read_sntp_status();
        break;
    case BLE_CMD_READ_WIFI_CONFIG:
        handle_read_wifi_config();
        break;
    case BLE_CMD_READ_MODBUS_POLL_INTV:
        handle_read_modbus_poll_interval();
        break;
    case BLE_CMD_WRITE_MODBUS_POLL_INTV:
        handle_write_modbus_poll_interval(data, data_len);
        break;
    case BLE_CMD_READ_MQTT_RECENT:
        handle_read_mqtt_recent();
        break;
    case BLE_CMD_OTA_START:
        handle_ota_start(data, data_len);
        break;
    case BLE_CMD_OTA_DATA:
        handle_ota_write_data(data, data_len);
        break;
    case BLE_CMD_OTA_EXECUTE:
        handle_ota_confirm_upgrade();
        break;
    default:
        ble_cmd_send_result_code(cmd, 0x0F);
        break;
    }
}

/* ============================================================ */
/* 帧解析                                                         */
/* ============================================================ */

static void parse_recv_buffer(void)
{
    while (recv_buf_len >= 6)
    {
        if (recv_buf[0] != BLE_FRAME_HEAD)
        {
            memmove(recv_buf, recv_buf + 1, --recv_buf_len);
            continue;
        }

        uint8_t cmd = recv_buf[1];
        uint16_t data_len = recv_buf[2] | (recv_buf[3] << 8);
        uint16_t frame_len = 6 + data_len;

        if (frame_len > RECV_BUF_SIZE)
        {
            memmove(recv_buf, recv_buf + 1, --recv_buf_len);
            continue;
        }

        if (recv_buf_len < frame_len)
        {
            break;
        }

        if (recv_buf[frame_len - 1] != BLE_FRAME_TAIL)
        {
            memmove(recv_buf, recv_buf + 1, --recv_buf_len);
            continue;
        }

        if (calc_checksum(recv_buf, frame_len - 2) != recv_buf[frame_len - 2])
        {
            memmove(recv_buf, recv_buf + 1, --recv_buf_len);
            continue;
        }

        dispatch_command(cmd, recv_buf + 4, data_len);

        if (recv_buf_len > frame_len)
        {
            memmove(recv_buf, recv_buf + frame_len, recv_buf_len - frame_len);
        }
        recv_buf_len -= frame_len;
    }
}

void ble_cmd_on_data_received(const uint8_t *data, uint16_t len)
{
    if (recv_buf_len + len > RECV_BUF_SIZE)
    {
        recv_buf_len = 0;
    }

    memcpy(recv_buf + recv_buf_len, data, len);
    recv_buf_len += len;
    parse_recv_buffer();
}

void ble_cmd_handler_init(void)
{
    ESP_LOGI(TAG, "BLE cmd handler init OK (Rev.5)");
}