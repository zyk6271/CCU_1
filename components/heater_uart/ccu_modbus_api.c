/**
 * @file ccu_modbus_api.c
 * @brief Modbus 主站: 轮询 / TCP上报 / 设备管理 / NTP校时 / 日志
 *
 * TCP 上报路径: xxx_info_upload() → ccu_modbus_tcp_frame_send()
 *   - 加密模式 (TG服务器/自定义TCP+加密): AES加密 → wifi_uart_write_frame
 *   - 明文模式 (自定义TCP+不加密):        直接 → wifi_uart_write_frame
 * MQTT: mqtt_service.c → mqtt_publish_report() → 明文JSON (不加密)
 */
#include "sdkconfig.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "mbcontroller.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include "ccu_modbus_api.h"
#include "ccu_modbus_config.h"
#include "crypto_aes.h"
#include "network_typedef.h"
#include "tcp_service.h"
#include "wifi_api.h"
#include "wifi_manager.h"
#include "system.h"

static const char *TAG = "ccu_modbus";

#define OPTS(min, max, step) { .opt1 = min, .opt2 = max, .opt3 = step }

/* ============================================================ */
/* CID 枚举                                                       */
/* ============================================================ */
enum {
    CID_GAS_FLOW_SENSOR = 0,
    CID_GAS_FLOW_SENSOR_TIME,
    CID_FRIDGE,
    CID_CURRENTWATCH,
    CID_DRYCONTACT,
    CID_DISHWASHER,
    CID_GAS_CONCENTRATION_SENSOR,
    CID_ULTRA_SONIC_GAS_METER,
    CID_ULTRA_SONIC_GAS_METER_TIME,
    CID_GAS_STEAMER,
    CID_GAS_WOK,
    CID_COUNT
};

/* dev_idx(0~8) → 主数据 CID */
static const uint8_t s_dev_idx_to_cid[MODBUS_DEVICE_TYPE_COUNT] = {
    CID_GAS_FLOW_SENSOR, CID_FRIDGE, CID_CURRENTWATCH, CID_DRYCONTACT,
    CID_DISHWASHER, CID_GAS_CONCENTRATION_SENSOR, CID_ULTRA_SONIC_GAS_METER,
    CID_GAS_STEAMER, CID_GAS_WOK,
};

/* ============================================================ */
/* 设备名称 / 默认地址 / 默认使能 (全部引用 config.h 宏)            */
/* ============================================================ */
static const char *s_device_names[MODBUS_DEVICE_TYPE_COUNT] = {
    "GAS_FLOW", "FRIDGE",   "CURRENTWA", "DRYCONTAC", "DISHWASHE",
    "GAS_CONC", "ULTRASONI", "STEAMER",  "WOK",
};


static const uint8_t s_device_default_enable[MODBUS_DEVICE_TYPE_COUNT] = {
    MB_DEF_EN_GAS_FLOW, MB_DEF_EN_FRIDGE, MB_DEF_EN_CURRENTWATCH,
    MB_DEF_EN_DRYCONTACT, MB_DEF_EN_DISHWASHER, MB_DEF_EN_GAS_CONC,
    MB_DEF_EN_ULTRA_SONIC, MB_DEF_EN_GAS_STEAMER, MB_DEF_EN_GAS_WOK,
};

/* ============================================================ */
/* 运行时状态                                                      */
/* ============================================================ */
static void *ccu_modbus_handle = NULL;

static ccu_modbus_device_status_t s_dev_status[MODBUS_DEVICE_TYPE_COUNT];
static uint8_t s_dev_enabled[MODBUS_DEVICE_TYPE_COUNT];

uint8_t modbus_detect_result = 0;
uint8_t modbus_device_valid[12] = {0};

uint8_t modbus_gas_flow_sensor_value[32]            = {0};
uint8_t modbus_fridge_value[64]                     = {0};
uint8_t modbus_currentwatch_value[16]               = {0};
uint8_t modbus_drycontact_value[16]                 = {0};
uint8_t modbus_dishwasher_value[128]                = {0};
uint8_t modbus_gas_concentration_sensor_value[32]   = {0};
uint8_t modbus_ultra_sonic_gas_meter_value[16]      = {0};
uint8_t modbus_gas_steamer_value[10]                = {0};
uint8_t modbus_gas_wok_value[8]                     = {0};

static uint8_t modbus_read_temp[128] = {0};

extern uint8_t smartconfig_start_flag;
static time_t last_ntp_sync_time = 0;

/* ============================================================ */
/* 通讯日志                                                       */
/* ============================================================ */
static ccu_modbus_log_entry_t s_log_ring[MODBUS_LOG_ENTRY_MAX];
static uint8_t s_log_head = 0, s_log_count = 0;

static uint8_t ccu_modbus_access_to_op(uint8_t mb_param_type, bool is_write)
{
    if (is_write) return 3;
    if (mb_param_type == MB_PARAM_INPUT)    return 1;
    if (mb_param_type == MB_PARAM_DISCRETE) return 2;
    return 0;
}

static void ccu_modbus_log_push(uint8_t dev_idx, uint8_t result, uint8_t op,
                                uint16_t reg_start, uint16_t reg_count)
{
    ccu_modbus_log_entry_t *e = &s_log_ring[s_log_head];
    time_t now; time(&now);
    e->timestamp  = (uint32_t)now;
    strncpy(e->device_name, s_device_names[dev_idx], MODBUS_LOG_NAME_MAX - 1);
    e->device_name[MODBUS_LOG_NAME_MAX - 1] = '\0';
    e->result     = result;
    e->slave_addr = ccu_modbus_device_addr_get(dev_idx);
    e->operation  = op;
    e->reg_start  = reg_start;
    e->reg_count  = reg_count;
    s_log_head = (s_log_head + 1) % MODBUS_LOG_ENTRY_MAX;
    if (s_log_count < MODBUS_LOG_ENTRY_MAX) s_log_count++;
}

/* ============================================================ */
/* NVS 使能/地址/从机地址 持久化                                    */
/* ============================================================ */
#define NVS_NS              "storage"
#define NVS_KEY_MB_DEV_EN   "mb_dev_en"
#define NVS_KEY_MB_DEV_REG  "mb_dev_reg"
#define NVS_KEY_MB_DEV_SLV  "mb_dev_slv"
#define NVS_KEY_MB_POLL_MS  "mb_poll_ms"

/* 轮询间隔 (ms), 运行时可修改, 轮询 task 每次循环末尾读取此值 */
static volatile uint32_t s_poll_interval_ms = MB_CFG_POLL_INTERVAL_MS;

static void ccu_modbus_enable_config_load(void)
{
    memcpy(s_dev_enabled, s_device_default_enable, sizeof(s_dev_enabled));
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(s_dev_enabled);
        if (nvs_get_blob(h, NVS_KEY_MB_DEV_EN, s_dev_enabled, &len) != ESP_OK)
            memcpy(s_dev_enabled, s_device_default_enable, sizeof(s_dev_enabled));
        nvs_close(h);
    }
}

static void ccu_modbus_enable_config_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, NVS_KEY_MB_DEV_EN, s_dev_enabled, sizeof(s_dev_enabled));
        nvs_commit(h); nvs_close(h);
    }
}

/* ============================================================ */
/* Modbus 参数描述符表 (运行时可改, 默认值全部引用宏)                */
/* ============================================================ */

/*
 * 与原始 device_parameters[] 字段完全一致, 仅硬编码改为宏引用.
 * 去掉 const 以支持运行时修改地址.
 */
static mb_parameter_descriptor_t device_parameters[] = {
    { CID_GAS_FLOW_SENSOR,          "GAS_FLOW_SENSOR",          "--", MB_DEF_ADDR_GAS_FLOW,     MB_PARAM_HOLDING,  MB_DEF_REG_GAS_FLOW_START,        MB_DEF_REG_GAS_FLOW_COUNT,        0, PARAM_TYPE_U16_BA,     MB_DEF_REG_GAS_FLOW_COUNT*2,        OPTS(0,0,0), PAR_PERMS_READ       },
    { CID_GAS_FLOW_SENSOR_TIME,     "GAS_FLOW_SENSOR_TIME",     "--", MB_DEF_ADDR_GAS_FLOW,     MB_PARAM_HOLDING,  MB_DEF_REG_GAS_FLOW_TIME_START,   MB_DEF_REG_GAS_FLOW_TIME_COUNT,   0, PARAM_TYPE_U16_BA,     MB_DEF_REG_GAS_FLOW_TIME_COUNT*2,   OPTS(0,0,0), PAR_PERMS_READ_WRITE },
    { CID_FRIDGE,                    "FRIDGE",                   "--", MB_DEF_ADDR_FRIDGE,       MB_PARAM_HOLDING,  MB_DEF_REG_FRIDGE_START,          MB_DEF_REG_FRIDGE_COUNT,          0, PARAM_TYPE_U16_BA,     MB_DEF_REG_FRIDGE_COUNT*2,          OPTS(0,0,0), PAR_PERMS_READ       },
    { CID_CURRENTWATCH,             "CURRENTWATCH",             "--", MB_DEF_ADDR_CURRENTWATCH, MB_PARAM_HOLDING,  MB_DEF_REG_CURRENTWATCH_START,    MB_DEF_REG_CURRENTWATCH_COUNT,    0, PARAM_TYPE_FLOAT_BADC, MB_DEF_REG_CURRENTWATCH_COUNT*2,    OPTS(0,0,0), PAR_PERMS_READ       },
    { CID_DRYCONTACT,               "DRYCONTACT",               "--", MB_DEF_ADDR_DRYCONTACT,   MB_PARAM_DISCRETE, MB_DEF_REG_DRYCONTACT_START,      MB_DEF_REG_DRYCONTACT_COUNT,      0, PARAM_TYPE_U8,         1,                                  OPTS(0,0,0), PAR_PERMS_READ       },
    { CID_DISHWASHER,               "DISHWASHER",               "--", MB_DEF_ADDR_DISHWASHER,   MB_PARAM_HOLDING,  MB_DEF_REG_DISHWASHER_START,      MB_DEF_REG_DISHWASHER_COUNT,      0, PARAM_TYPE_U16_BA,     MB_DEF_REG_DISHWASHER_COUNT*2,      OPTS(0,0,0), PAR_PERMS_READ       },
    { CID_GAS_CONCENTRATION_SENSOR, "GAS_CONCENTRATION_SENSOR", "--", MB_DEF_ADDR_GAS_CONC,     MB_PARAM_HOLDING,  MB_DEF_REG_GAS_CONC_START,        MB_DEF_REG_GAS_CONC_COUNT,        0, PARAM_TYPE_U16_BA,     MB_DEF_REG_GAS_CONC_COUNT*2,        OPTS(0,0,0), PAR_PERMS_READ       },
    { CID_ULTRA_SONIC_GAS_METER,    "ULTRA_SONIC_GAS_METER",    "--", MB_DEF_ADDR_ULTRA_SONIC,  MB_PARAM_HOLDING,  MB_DEF_REG_ULTRA_SONIC_START,     MB_DEF_REG_ULTRA_SONIC_COUNT,     0, PARAM_TYPE_U16_BA,     MB_DEF_REG_ULTRA_SONIC_COUNT*2,     OPTS(0,0,0), PAR_PERMS_READ       },
    { CID_ULTRA_SONIC_GAS_METER_TIME,"ULTRA_SONIC_GAS_METER_T", "--", MB_DEF_ADDR_ULTRA_SONIC,  MB_PARAM_HOLDING,  MB_DEF_REG_ULTRA_SONIC_TIME_START,MB_DEF_REG_ULTRA_SONIC_TIME_COUNT,0, PARAM_TYPE_U16_BA,     MB_DEF_REG_ULTRA_SONIC_TIME_COUNT*2,OPTS(0,0,0), PAR_PERMS_READ_WRITE },
    { CID_GAS_STEAMER,              "GAS_STEAMER",              "--", MB_DEF_ADDR_GAS_STEAMER,  MB_PARAM_HOLDING,  MB_DEF_REG_GAS_STEAMER_START,     MB_DEF_REG_GAS_STEAMER_COUNT,     0, PARAM_TYPE_U16_BA,     MB_DEF_REG_GAS_STEAMER_COUNT*2,     OPTS(0,0,0), PAR_PERMS_READ       },
    { CID_GAS_WOK,                  "GAS_WOK",                  "--", MB_DEF_ADDR_GAS_WOK,      MB_PARAM_HOLDING,  MB_DEF_REG_GAS_WOK_START,         MB_DEF_REG_GAS_WOK_COUNT,         0, PARAM_TYPE_U16_BA,     MB_DEF_REG_GAS_WOK_COUNT*2,         OPTS(0,0,0), PAR_PERMS_READ       },
};
static const uint16_t num_device_parameters = sizeof(device_parameters)/sizeof(device_parameters[0]);

/* ============================================================ */
/* NVS 寄存器地址 / 从机地址 持久化                                 */
/* ============================================================ */
typedef struct { uint16_t start; uint16_t count; } ccu_modbus_reg_entry_t;

static void ccu_modbus_reg_config_load(void)
{
    nvs_handle_t h;
    ccu_modbus_reg_entry_t a[CID_COUNT];
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(a);
        if (nvs_get_blob(h, NVS_KEY_MB_DEV_REG, a, &len) == ESP_OK && len == sizeof(a)) {
            for (int i = 0; i < CID_COUNT; i++) {
                if (a[i].count == 0 || a[i].count > 125) continue;
                device_parameters[i].mb_reg_start = a[i].start;
                device_parameters[i].mb_size      = a[i].count;
                if (device_parameters[i].mb_param_type != MB_PARAM_DISCRETE)
                    device_parameters[i].param_size = a[i].count * 2;
                ESP_LOGI(TAG, "CID[%d] reg=0x%04X cnt=%d (NVS)", i, a[i].start, a[i].count);
            }
        }
        nvs_close(h);
    }
}

static void ccu_modbus_reg_config_save(void)
{
    nvs_handle_t h;
    ccu_modbus_reg_entry_t a[CID_COUNT];
    for (int i = 0; i < CID_COUNT; i++) {
        a[i].start = device_parameters[i].mb_reg_start;
        a[i].count = device_parameters[i].mb_size;
    }
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, NVS_KEY_MB_DEV_REG, a, sizeof(a));
        nvs_commit(h); nvs_close(h);
    }
}

static void ccu_modbus_slave_addr_config_load(void)
{
    nvs_handle_t h;
    uint8_t addrs[MODBUS_DEVICE_TYPE_COUNT];
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(addrs);
        if (nvs_get_blob(h, NVS_KEY_MB_DEV_SLV, addrs, &len) == ESP_OK && len == sizeof(addrs)) {
            for (int i = 0; i < MODBUS_DEVICE_TYPE_COUNT; i++) {
                if (addrs[i] < 1 || addrs[i] > 247) continue;
                uint8_t cid = s_dev_idx_to_cid[i];
                device_parameters[cid].mb_slave_addr = addrs[i];
                /* TIME CID 也同步 (GAS_FLOW_TIME 跟 GAS_FLOW 同从机) */
                if (cid == CID_GAS_FLOW_SENSOR)
                    device_parameters[CID_GAS_FLOW_SENSOR_TIME].mb_slave_addr = addrs[i];
                else if (cid == CID_ULTRA_SONIC_GAS_METER)
                    device_parameters[CID_ULTRA_SONIC_GAS_METER_TIME].mb_slave_addr = addrs[i];
                ESP_LOGI(TAG, "Dev[%d] slave_addr=%d (NVS)", i, addrs[i]);
            }
        }
        nvs_close(h);
    }
}

static void ccu_modbus_slave_addr_config_save(void)
{
    nvs_handle_t h;
    uint8_t addrs[MODBUS_DEVICE_TYPE_COUNT];
    for (int i = 0; i < MODBUS_DEVICE_TYPE_COUNT; i++)
        addrs[i] = device_parameters[s_dev_idx_to_cid[i]].mb_slave_addr;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, NVS_KEY_MB_DEV_SLV, addrs, sizeof(addrs));
        nvs_commit(h); nvs_close(h);
    }
}

static void ccu_modbus_descriptor_reload(void)
{
    esp_err_t err = mbc_master_set_descriptor(ccu_modbus_handle, device_parameters, num_device_parameters);
    if (err != ESP_OK) ESP_LOGE(TAG, "set_descriptor fail (0x%x)", (int)err);
}

/* ============================================================ */
/* 公开管理接口                                                    */
/* ============================================================ */
uint8_t modbus_detect_result_read(void) { return modbus_detect_result; }

void ccu_modbus_device_status_get_all(ccu_modbus_device_status_t *out)
{
    for (int i = 0; i < MODBUS_DEVICE_TYPE_COUNT; i++) {
        out[i] = s_dev_status[i];
        out[i].enabled   = s_dev_enabled[i];
        uint8_t cid      = s_dev_idx_to_cid[i];
        out[i].reg_start = device_parameters[cid].mb_reg_start;
        out[i].reg_count = device_parameters[cid].mb_size;
    }
}

const char *ccu_modbus_device_name_get(uint8_t i)
{
    return (i < MODBUS_DEVICE_TYPE_COUNT) ? s_device_names[i] : "UNKNOWN";
}

uint8_t ccu_modbus_device_addr_get(uint8_t i)
{
    if (i >= MODBUS_DEVICE_TYPE_COUNT) return 0xFF;
    return device_parameters[s_dev_idx_to_cid[i]].mb_slave_addr;
}

int ccu_modbus_device_enable_set(uint8_t i, uint8_t en)
{
    if (i >= MODBUS_DEVICE_TYPE_COUNT) return -1;
    s_dev_enabled[i] = en ? 1 : 0;
    ccu_modbus_enable_config_save();
    return 0;
}

int ccu_modbus_device_reg_addr_set(uint8_t index, uint16_t reg_start, uint16_t reg_count)
{
    uint8_t cid;
    if (index < MODBUS_DEVICE_TYPE_COUNT) {
        cid = s_dev_idx_to_cid[index];
    } else if (index == 9) {
        cid = CID_GAS_FLOW_SENSOR_TIME;
    } else if (index == 10) {
        cid = CID_ULTRA_SONIC_GAS_METER_TIME;
    } else {
        return -1;
    }
    if (reg_count == 0 || reg_count > 125) return -2;

    device_parameters[cid].mb_reg_start = reg_start;
    device_parameters[cid].mb_size      = reg_count;
    if (device_parameters[cid].mb_param_type != MB_PARAM_DISCRETE)
        device_parameters[cid].param_size = reg_count * 2;

    ccu_modbus_reg_config_save();
    ccu_modbus_descriptor_reload();
    ESP_LOGI(TAG, "CID[%d] reg→0x%04X cnt→%d", cid, reg_start, reg_count);
    return 0;
}

int ccu_modbus_device_slave_addr_set(uint8_t index, uint8_t slave_addr)
{
    if (index >= MODBUS_DEVICE_TYPE_COUNT) return -1;
    if (slave_addr < 1 || slave_addr > 247) return -2;

    uint8_t cid = s_dev_idx_to_cid[index];
    device_parameters[cid].mb_slave_addr = slave_addr;
    /* TIME CID 也同步 */
    if (cid == CID_GAS_FLOW_SENSOR)
        device_parameters[CID_GAS_FLOW_SENSOR_TIME].mb_slave_addr = slave_addr;
    else if (cid == CID_ULTRA_SONIC_GAS_METER)
        device_parameters[CID_ULTRA_SONIC_GAS_METER_TIME].mb_slave_addr = slave_addr;

    ccu_modbus_slave_addr_config_save();
    ccu_modbus_descriptor_reload();
    ESP_LOGI(TAG, "Dev[%d] slave→%d", index, slave_addr);
    return 0;
}

uint8_t ccu_modbus_log_read(ccu_modbus_log_entry_t *out, uint8_t max_cnt)
{
    uint8_t cnt = (s_log_count < max_cnt) ? s_log_count : max_cnt;
    for (uint8_t i = 0; i < cnt; i++) {
        uint8_t idx = (s_log_head + MODBUS_LOG_ENTRY_MAX - 1 - i) % MODBUS_LOG_ENTRY_MAX;
        out[i] = s_log_ring[idx];
    }
    return cnt;
}

/* ============================================================ */
/* TCP 上报公共函数 (根据配置决定是否 AES 加密)                     */
/* ============================================================ */
static void ccu_modbus_tcp_frame_send(const uint8_t *plain, uint16_t plain_len)
{
    if (tcp_service_encrypt_enabled_get())
    {
        /* 加密模式: TG 服务器始终走这里, 自定义 TCP 可选 */
        uint8_t *enc_ptr;
        uint32_t enc_size;
        crypto_aes_remote_encrypt((uint8_t *)plain, plain_len, &enc_ptr, &enc_size);
        uint16_t slen = set_wifi_uart_buffer(0, enc_ptr, enc_size);
        wifi_uart_write_frame(0xB0, plain_len, slen);
        free(enc_ptr);
    }
    else
    {
        /* 明文模式: 仅自定义 TCP 且 encrypt_enabled=0 时 */
        uint16_t slen = set_wifi_uart_buffer(0, plain, plain_len);
        wifi_uart_write_frame(0xB0, plain_len, slen);
    }
}

/* ============================================================ */
/* 各设备 TCP 上报 (根据 encrypt_enabled 决定是否加密)              */
/* ============================================================ */
void gas_flow_sensor_info_upload(void)
{
    uint8_t buf[41] = {0};
    static int total_flow, total_flow_past = 0;
    total_flow = (modbus_gas_flow_sensor_value[6]*0x100000) + (modbus_gas_flow_sensor_value[7]*0x10000)
               + (modbus_gas_flow_sensor_value[8]*0x1000) + (modbus_gas_flow_sensor_value[9]*0x100)
               + (modbus_gas_flow_sensor_value[10]*0x10) + modbus_gas_flow_sensor_value[11];
    if (total_flow_past == 0 && total_flow != total_flow_past) total_flow_past = total_flow;
    buf[0] = tcp_send_count_read();
    memcpy(&buf[1], modbus_gas_flow_sensor_value, 26);
    uint16_t diff = (uint16_t)((total_flow - total_flow_past) * 0.135);
    buf[30] = (diff >> 8) & 0xFF; buf[31] = diff & 0xFF;
    ESP_LOG_BUFFER_HEXDUMP("gas_flow_upload", buf, 41, ESP_LOG_INFO);
    ccu_modbus_tcp_frame_send(buf, 41);
}

void fridge_info_upload(void)
{
    uint8_t buf[41] = {0};
    float temp = (modbus_fridge_value[0] << 8) | modbus_fridge_value[1];
    buf[0] = tcp_send_count_read();
    buf[2] = modbus_fridge_value[0]; buf[3] = modbus_fridge_value[1];
    buf[35] = modbus_fridge_value[27]; buf[40] = modbus_fridge_value[25];
    if (modbus_fridge_value[35]) buf[1] = 0x08;
    if (modbus_fridge_value[11]) buf[1] = 0x07;
    if (modbus_fridge_value[9])  buf[1] = 0x06;
    if (modbus_fridge_value[5])  buf[1] = 0x05;
    if (temp < 0) buf[36] = 1;
    ESP_LOG_BUFFER_HEXDUMP("fridge_upload", buf, 41, ESP_LOG_INFO);
    ccu_modbus_tcp_frame_send(buf, 41);
}

void currentwatch_info_upload(void)
{
    uint8_t buf[41] = {0};
    buf[0] = tcp_send_count_read();
    buf[2]=modbus_currentwatch_value[0]; buf[3]=modbus_currentwatch_value[1];
    buf[4]=modbus_currentwatch_value[2]; buf[5]=modbus_currentwatch_value[3];
    buf[13]=modbus_currentwatch_value[4]; buf[14]=modbus_currentwatch_value[5];
    buf[16]=modbus_currentwatch_value[6]; buf[17]=modbus_currentwatch_value[7];
    buf[30]=modbus_currentwatch_value[8]; buf[31]=modbus_currentwatch_value[9];
    buf[33]=modbus_currentwatch_value[10]; buf[34]=modbus_currentwatch_value[11];
    ESP_LOG_BUFFER_HEXDUMP("currentwatch_upload", buf, 41, ESP_LOG_INFO);
    ccu_modbus_tcp_frame_send(buf, 41);
}

void drycontact_sensor_info_upload(void)
{
    uint8_t buf[41] = {0};
    buf[0] = tcp_send_count_read();
    buf[35] = (modbus_drycontact_value[0] & 0x02) ? 1 : 0;
    buf[38] = (modbus_drycontact_value[0] & 0x04) ? 1 : 0;
    buf[40] = (modbus_drycontact_value[0] & 0x01) ? 1 : 0;
    ESP_LOG_BUFFER_HEXDUMP("drycontact_upload", buf, 41, ESP_LOG_INFO);
    ccu_modbus_tcp_frame_send(buf, 41);
}

void dish_washer_info_upload(void)
{
    uint8_t buf[41] = {0}, t[41] = {0};
    memcpy(&t[0],  modbus_dishwasher_value+(0x1321-0x1320)*2, 2);
    memcpy(&t[2],  modbus_dishwasher_value+(0x1324-0x1320)*2, 8);
    memcpy(&t[10], modbus_dishwasher_value+(0x132C-0x1320)*2, 22);
    memcpy(&t[32], modbus_dishwasher_value+(0x133B-0x1320)*2, 6);
    memcpy(&t[38], modbus_dishwasher_value+(0x1349-0x1320)*2, 2);
    buf[0]=tcp_send_count_read();
    buf[2]=t[2]; buf[3]=t[3]; buf[4]=t[4]; buf[5]=t[5];
    buf[6]=t[10]; buf[7]=t[11]; buf[10]=t[0]; buf[11]=t[1];
    buf[13]=t[32]; buf[14]=t[33]; buf[16]=t[28]; buf[17]=t[29];
    buf[30]=t[22]; buf[31]=t[23]; buf[33]=t[30]; buf[34]=t[31];
    ESP_LOG_BUFFER_HEXDUMP("dishwasher_upload", buf, 41, ESP_LOG_INFO);
    ccu_modbus_tcp_frame_send(buf, 41);
}

void gas_concentration_sensor_info_upload(void)
{
    uint8_t buf[41] = {0};
    float temp = (modbus_gas_concentration_sensor_value[16]<<8)|modbus_gas_concentration_sensor_value[17];
    float humi = (modbus_gas_concentration_sensor_value[18]<<8)|modbus_gas_concentration_sensor_value[19];
    buf[0] = tcp_send_count_read();
    buf[2]=modbus_gas_concentration_sensor_value[0]; buf[3]=modbus_gas_concentration_sensor_value[1];
    buf[4]=modbus_gas_concentration_sensor_value[16]; buf[5]=modbus_gas_concentration_sensor_value[17];
    buf[6]=modbus_gas_concentration_sensor_value[18]; buf[7]=modbus_gas_concentration_sensor_value[19];
    if (humi < 0) buf[36] = 1;
    if (temp < 0) buf[40] = 1;
    ESP_LOG_BUFFER_HEXDUMP("gas_conc_upload", buf, 41, ESP_LOG_INFO);
    ccu_modbus_tcp_frame_send(buf, 41);
}

void ultra_sonic_gas_meter_info_upload(void)
{
    uint8_t buf[41] = {0};
    buf[0] = tcp_send_count_read();
    memcpy(&buf[1], modbus_ultra_sonic_gas_meter_value, 16);
    ESP_LOG_BUFFER_HEXDUMP("ultrasonic_upload", buf, 41, ESP_LOG_INFO);
    ccu_modbus_tcp_frame_send(buf, 41);
}

void gas_steamer_info_upload(void) {
    uint8_t buf[41]={0}; buf[0]=tcp_send_count_read();
    memcpy(&buf[1], modbus_gas_steamer_value, 10);
    ESP_LOG_BUFFER_HEXDUMP("steamer_upload", buf, 41, ESP_LOG_INFO);
    ccu_modbus_tcp_frame_send(buf, 41);
}

void gas_wok_info_upload(void) {
    uint8_t buf[41]={0}; buf[0]=tcp_send_count_read();
    memcpy(&buf[1], modbus_gas_wok_value, 8);
    ESP_LOG_BUFFER_HEXDUMP("wok_upload", buf, 41, ESP_LOG_INFO);
    ccu_modbus_tcp_frame_send(buf, 41);
}

void ccu_poll_status_reset(void)
{
    memset(modbus_gas_flow_sensor_value, 0, sizeof(modbus_gas_flow_sensor_value));
    memset(modbus_fridge_value, 0, sizeof(modbus_fridge_value));
    memset(modbus_currentwatch_value, 0, sizeof(modbus_currentwatch_value));
    memset(modbus_drycontact_value, 0, sizeof(modbus_drycontact_value));
    memset(modbus_dishwasher_value, 0, sizeof(modbus_dishwasher_value));
    memset(modbus_gas_concentration_sensor_value, 0, sizeof(modbus_gas_concentration_sensor_value));
    memset(modbus_ultra_sonic_gas_meter_value, 0, sizeof(modbus_ultra_sonic_gas_meter_value));
    memset(modbus_gas_steamer_value, 0, sizeof(modbus_gas_steamer_value));
    memset(modbus_gas_wok_value, 0, sizeof(modbus_gas_wok_value));
}

/* ============================================================ */
/* NTP 时间同步 (向从设备写入当前时间)                              */
/* ============================================================ */
static int decimal_to_bcd_convert(int dec) { return dec + (dec/10)*6; }

void ccu_modbus_ntp_sync_gas_flow_sensor(void)
{
    uint8_t type = 0; time_t now; struct tm ti; uint8_t td[8]={0};
    time(&now); localtime_r(&now, &ti);
    td[1]=ti.tm_year+1900-2000; td[2]=ti.tm_mon+1; td[3]=ti.tm_mday;
    td[4]=(ti.tm_wday==0)?7:ti.tm_wday;
    td[5]=ti.tm_hour; td[6]=ti.tm_min; td[7]=ti.tm_sec;
    const mb_parameter_descriptor_t *pd = NULL;
    esp_err_t err = mbc_master_get_cid_info(ccu_modbus_handle, CID_GAS_FLOW_SENSOR_TIME, &pd);
    if ((err != ESP_ERR_NOT_FOUND) && (pd != NULL)) {
        err = mbc_master_set_parameter(ccu_modbus_handle, pd->cid, td, &type);
        if (err != ESP_OK) ESP_LOGW(TAG, "NTP sync gas_flow fail (0x%x)", (int)err);
    }
}

void ccu_modbus_ntp_sync_ultrasonic_gas_meter(void)
{
    uint8_t type = 0; time_t now; struct tm ti; uint8_t td[6]={0};
    time(&now); localtime_r(&now, &ti);
    td[0]=decimal_to_bcd_convert(ti.tm_year+1900-2000);
    td[1]=decimal_to_bcd_convert(ti.tm_mon+1);
    td[2]=decimal_to_bcd_convert(ti.tm_mday);
    td[3]=decimal_to_bcd_convert(ti.tm_hour);
    td[4]=decimal_to_bcd_convert(ti.tm_min);
    td[5]=decimal_to_bcd_convert(ti.tm_sec);
    const mb_parameter_descriptor_t *pd = NULL;
    esp_err_t err = mbc_master_get_cid_info(ccu_modbus_handle, CID_ULTRA_SONIC_GAS_METER_TIME, &pd);
    if ((err != ESP_ERR_NOT_FOUND) && (pd != NULL)) {
        err = mbc_master_set_parameter(ccu_modbus_handle, pd->cid, td, &type);
        if (err != ESP_OK) ESP_LOGW(TAG, "NTP sync ultrasonic fail (0x%x)", (int)err);
    }
}

void ccu_modbus_ntp_sync(void)
{
    /* 开关关闭时不向从设备写入时间 */
    extern uint8_t network_sntp_enable_get(void);
    if (!network_sntp_enable_get()) return;

    time_t now; struct tm timeinfo;
    time(&now); localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year > (2020 - 1900)) {
        if (last_ntp_sync_time == 0 || (now - last_ntp_sync_time) >= 24 * 60 * 60) {
            ccu_modbus_ntp_sync_gas_flow_sensor();
            vTaskDelay(pdMS_TO_TICKS(MB_CFG_POLL_DEVICE_DELAY_MS));
            ccu_modbus_ntp_sync_ultrasonic_gas_meter();
            vTaskDelay(pdMS_TO_TICKS(MB_CFG_POLL_DEVICE_DELAY_MS));
            time(&last_ntp_sync_time);
        }
    }
}

/* ============================================================ */
/* 轮询核心 (保持原始函数名 ccu_modbus_poll_select)                */
/* ============================================================ */
void ccu_modbus_poll_select(uint8_t modbus_cid, uint8_t *value_temp,
                            uint8_t device_valid_id, void (*mb_callback)(void))
{
    uint8_t type = 0;
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t *param_descriptor = NULL;

    if (smartconfig_start_flag != 0) return;

    /* Rev.3: 设备使能检查 */
    if (device_valid_id < MODBUS_DEVICE_TYPE_COUNT && !s_dev_enabled[device_valid_id]) {
        s_dev_status[device_valid_id].status = 0;
        return;
    }

    err = mbc_master_get_cid_info(ccu_modbus_handle, modbus_cid, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
    {
        uint16_t rs = param_descriptor->mb_reg_start;
        uint16_t rc = param_descriptor->mb_size;
        uint8_t  op = ccu_modbus_access_to_op(param_descriptor->mb_param_type, false);

        memset(modbus_read_temp, 0, sizeof(modbus_read_temp));
        err = mbc_master_get_parameter(ccu_modbus_handle, param_descriptor->cid,
                                       modbus_read_temp, &type);
        if (err == ESP_OK)
        {
            modbus_device_valid[device_valid_id] = 1;
            modbus_detect_result = 1;

            if (device_valid_id < MODBUS_DEVICE_TYPE_COUNT) {
                s_dev_status[device_valid_id].status = 1;
                s_dev_status[device_valid_id].ok_count++;
                ccu_modbus_log_push(device_valid_id, 0, op, rs, rc);
            }

            if (memcmp(value_temp, modbus_read_temp, param_descriptor->param_size) == 0) {
                ESP_LOGD(TAG, "%s modbus value no change", param_descriptor->param_key);
            } else {
                memcpy(value_temp, modbus_read_temp, param_descriptor->param_size);
                ESP_LOGI(TAG, "%s modbus value has change", param_descriptor->param_key);
#if HEATER_CUSTOM_SERVER == 0
                mb_callback();
#endif
            }
        }
        else
        {
            if (device_valid_id < MODBUS_DEVICE_TYPE_COUNT) {
                uint8_t fail_code = (err == ESP_ERR_TIMEOUT) ? 1 : 3;
                s_dev_status[device_valid_id].status = (err == ESP_ERR_TIMEOUT) ? 2 : 4;
                s_dev_status[device_valid_id].err_count++;
                ccu_modbus_log_push(device_valid_id, fail_code, op, rs, rc);
                ESP_LOGW(TAG, "%s poll fail 0x%x @0x%04X[%d]",
                         param_descriptor->param_key, (int)err, rs, rc);
            }
        }
    }
    vTaskDelay(pdMS_TO_TICKS(MB_CFG_POLL_DEVICE_DELAY_MS));
}

void ccu_modbus_poll(void)
{
    ccu_modbus_ntp_sync();   /* ★ Rev.3: 打开 NTP 校时 */
    ccu_modbus_poll_select(CID_GAS_FLOW_SENSOR, modbus_gas_flow_sensor_value, 0, gas_flow_sensor_info_upload);
    ccu_modbus_poll_select(CID_FRIDGE, modbus_fridge_value, 1, fridge_info_upload);
    ccu_modbus_poll_select(CID_CURRENTWATCH, modbus_currentwatch_value, 2, currentwatch_info_upload);
    ccu_modbus_poll_select(CID_DRYCONTACT, modbus_drycontact_value, 3, drycontact_sensor_info_upload);
    ccu_modbus_poll_select(CID_DISHWASHER, modbus_dishwasher_value, 4, dish_washer_info_upload);
    ccu_modbus_poll_select(CID_GAS_CONCENTRATION_SENSOR, modbus_gas_concentration_sensor_value, 5, gas_concentration_sensor_info_upload);
    ccu_modbus_poll_select(CID_ULTRA_SONIC_GAS_METER, modbus_ultra_sonic_gas_meter_value, 6, ultra_sonic_gas_meter_info_upload);
    ccu_modbus_poll_select(CID_GAS_STEAMER, modbus_gas_steamer_value, 7, gas_steamer_info_upload);
    ccu_modbus_poll_select(CID_GAS_WOK, modbus_gas_wok_value, 8, gas_wok_info_upload);
}

void wifi_ccu_modbus_poll_upload(void)
{
    ESP_LOG_BUFFER_HEXDUMP("wifi_ccu_modbus_poll_upload", modbus_device_valid, 12, ESP_LOG_INFO);
    if (modbus_device_valid[0]) gas_flow_sensor_info_upload();
    if (modbus_device_valid[1]) fridge_info_upload();
    if (modbus_device_valid[2]) currentwatch_info_upload();
    if (modbus_device_valid[3]) drycontact_sensor_info_upload();
    if (modbus_device_valid[4]) dish_washer_info_upload();
    if (modbus_device_valid[5]) gas_concentration_sensor_info_upload();
    if (modbus_device_valid[6]) ultra_sonic_gas_meter_info_upload();
    if (modbus_device_valid[7]) gas_steamer_info_upload();
    if (modbus_device_valid[8]) gas_wok_info_upload();
}

void modbus_poll_thread_callback(void *parameter)
{
    while (1) {
        ccu_modbus_poll();
        vTaskDelay(pdMS_TO_TICKS(s_poll_interval_ms));
    }
}

static void ccu_modbus_poll_interval_load(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        uint32_t v = 0;
        if (nvs_get_u32(h, NVS_KEY_MB_POLL_MS, &v) == ESP_OK && v >= 500 && v <= 10000)
            s_poll_interval_ms = v;
        nvs_close(h);
    }
}

uint32_t ccu_modbus_poll_interval_get(void)
{
    return s_poll_interval_ms;
}

int ccu_modbus_poll_interval_set(uint32_t ms)
{
    if (ms < 500 || ms > 10000) return -1;
    s_poll_interval_ms = ms;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, NVS_KEY_MB_POLL_MS, ms);
        nvs_commit(h); nvs_close(h);
    }
    ESP_LOGI(TAG, "Poll interval set to %lu ms", (unsigned long)ms);
    return 0;
}

/* ============================================================ */
/* 初始化                                                         */
/* ============================================================ */
void ccu_modbus_init(void)
{
    memset(s_dev_status, 0, sizeof(s_dev_status));
    ccu_modbus_enable_config_load();
    ccu_modbus_reg_config_load();
    ccu_modbus_slave_addr_config_load();
    ccu_modbus_poll_interval_load();

    mb_communication_info_t comm = {
        .ser_opts.port          = MB_CFG_PORT_NUM,
        .ser_opts.mode          = MB_RTU,
        .ser_opts.baudrate      = MB_CFG_BAUD_RATE,
        .ser_opts.parity        = MB_PARITY_NONE,
        .ser_opts.uid           = 0,
        .ser_opts.response_tout_ms = MB_CFG_RESPONSE_TOUT_MS,
        .ser_opts.data_bits     = UART_DATA_8_BITS,
        .ser_opts.stop_bits     = UART_STOP_BITS_1,
    };

    esp_err_t err = mbc_master_create_serial(&comm, &ccu_modbus_handle);
    if (err != ESP_OK) ESP_LOGE(TAG, "mb controller initialization fail, returns(0x%x).", (int)err);

    err = uart_set_pin(MB_CFG_PORT_NUM, MB_CFG_TXD_PIN, MB_CFG_RXD_PIN, MB_CFG_RE_PIN, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) ESP_LOGE(TAG, "uart_set_pin fail, returns(0x%x).", (int)err);

    err = uart_set_mode(MB_CFG_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    if (err != ESP_OK) ESP_LOGE(TAG, "uart_set_mode fail, returns(0x%x).", (int)err);

    vTaskDelay(5);
    err = mbc_master_set_descriptor(ccu_modbus_handle, &device_parameters[0], num_device_parameters);
    if (err != ESP_OK) ESP_LOGE(TAG, "mbc_master_set_descriptor fail, returns(0x%x).", (int)err);

    err = mbc_master_start(ccu_modbus_handle);
    if (err != ESP_OK) ESP_LOGE(TAG, "mbc_master_start fail, returns(0x%x).", (int)err);

    xTaskCreate(modbus_poll_thread_callback, "modbus_poll_thread_handle", 8192, NULL, 5, NULL);
}