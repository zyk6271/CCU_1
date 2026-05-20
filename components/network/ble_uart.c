/**
 * @file ble_uart.c
 * @brief ESP32 BLE UART 服务 (Nordic NUS 风格)
 *
 * NUS UUID:
 *   Service:  6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 *   RX (写):  6E400002-...  App -> Device
 *   TX (通知): 6E400003-...  Device -> App
 *
 * 设备广播名称: CTM=XXXXXXXXXXXX (MAC 12位)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_mac.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_timer.h"

#include "ble_uart.h"
#include "ble_cmd_handler.h"

#define TAG "ble_uart"

/* 5 分钟空闲断连 (微秒) */
#define BLE_IDLE_TIMEOUT_US  (5 * 60 * 1000000ULL)

/* NUS UUID (128-bit 小端) */
static uint8_t nus_service_uuid[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E,
};
static uint8_t nus_rx_char_uuid[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E,
};
static uint8_t nus_tx_char_uuid[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E,
};

/* GATT 属性表索引 */
#define NUS_APP_ID     0x55
#define CHAR_VAL_MAX   BLE_MTU_MAX   /* 匹配最大 MTU */

enum
{
    ATTR_IDX_SERVICE,
    ATTR_IDX_RX_CHAR_DECL,
    ATTR_IDX_RX_CHAR_VAL,
    ATTR_IDX_TX_CHAR_DECL,
    ATTR_IDX_TX_CHAR_VAL,
    ATTR_IDX_TX_CHAR_CCC,
    ATTR_IDX_COUNT,
};

static uint16_t attr_handle_table[ATTR_IDX_COUNT];

static const uint16_t uuid_primary_service = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t uuid_char_declare    = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t uuid_client_config   = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t  char_prop_write      = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t  char_prop_notify     = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static uint8_t client_config_val[2] = {0};

static const esp_gatts_attr_db_t nus_attr_table[ATTR_IDX_COUNT] = {
    [ATTR_IDX_SERVICE] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&uuid_primary_service, ESP_GATT_PERM_READ,
          16, 16, nus_service_uuid}},

    [ATTR_IDX_RX_CHAR_DECL] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&uuid_char_declare, ESP_GATT_PERM_READ,
          1, 1, (uint8_t *)&char_prop_write}},

    [ATTR_IDX_RX_CHAR_VAL] =
        {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_128, nus_rx_char_uuid, ESP_GATT_PERM_WRITE,
          CHAR_VAL_MAX, 0, NULL}},

    [ATTR_IDX_TX_CHAR_DECL] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&uuid_char_declare, ESP_GATT_PERM_READ,
          1, 1, (uint8_t *)&char_prop_notify}},

    [ATTR_IDX_TX_CHAR_VAL] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, nus_tx_char_uuid, ESP_GATT_PERM_READ,
          CHAR_VAL_MAX, 0, NULL}},

    [ATTR_IDX_TX_CHAR_CCC] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&uuid_client_config,
          ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2, 2, client_config_val}},
};

/* 连接状态 */
static esp_gatt_if_t s_gatt_if       = ESP_GATT_IF_NONE;
static uint16_t      s_conn_id       = 0xFFFF;
static bool          s_is_connected  = false;
static bool          s_notify_enabled = false;
static uint16_t      s_negotiated_mtu = 23;  /* BLE 默认 MTU */

/* 断开后需要重启广播的标志 */
static volatile bool s_pending_restart_adv = false;

/* 空闲断连定时器 */
static esp_timer_handle_t s_idle_timer = NULL;

/* 设备名 */
static char s_device_name[32];

/* 广播数据配置完成标志 */
#define ADV_DATA_FLAG  (1 << 0)
#define RSP_DATA_FLAG  (1 << 1)
static uint8_t s_adv_config_done = 0;

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp    = false,
    .include_name    = true,
    .include_txpower = true,
    .min_interval    = 0x0006,
    .max_interval    = 0x0010,
    .service_uuid_len = 16,
    .p_service_uuid  = nus_service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp    = true,
    .include_name    = true,
    .include_txpower = true,
    .min_interval    = 0x0006,
    .max_interval    = 0x0010,
    .service_uuid_len = 16,
    .p_service_uuid  = nus_service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min       = 0x20,
    .adv_int_max       = 0x40,
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* ============================================================ */
/* 空闲断连                                                       */
/* ============================================================ */

static void idle_timer_callback(void *arg)
{
    if (s_is_connected)
    {
        ESP_LOGW(TAG, "BLE idle timeout (5 min), disconnecting");
        esp_ble_gap_disconnect(*(esp_bd_addr_t *)arg);
    }
}

/* 保存当前连接的远端地址, 供 idle timer 断开使用 */
static esp_bd_addr_t s_remote_bda;

static void idle_timer_start(void)
{
    if (s_idle_timer)
    {
        esp_timer_stop(s_idle_timer);  /* 忽略未运行时的错误 */
        esp_timer_start_once(s_idle_timer, BLE_IDLE_TIMEOUT_US);
    }
}

static void idle_timer_stop(void)
{
    if (s_idle_timer)
    {
        esp_timer_stop(s_idle_timer);
    }
}

void ble_uart_activity_reset(void)
{
    if (s_is_connected)
    {
        idle_timer_start();
    }
}

void ble_uart_disconnect(void)
{
    if (s_is_connected && s_gatt_if != ESP_GATT_IF_NONE)
    {
        esp_ble_gap_disconnect(s_remote_bda);
    }
}

/* ============================================================ */
/* GAP 回调                                                       */
/* ============================================================ */

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        s_adv_config_done &= ~ADV_DATA_FLAG;
        if (s_adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;

    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        s_adv_config_done &= ~RSP_DATA_FLAG;
        if (s_adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(TAG, "Advertising started");
        }
        else
        {
            ESP_LOGE(TAG, "Advertising start failed, status=%d", param->adv_start_cmpl.status);
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGI(TAG, "Advertising stopped");
        if (s_pending_restart_adv)
        {
            s_pending_restart_adv = false;
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;

    default:
        break;
    }
}

/* ============================================================ */
/* GATTS 回调                                                     */
/* ============================================================ */

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatt_if,
                                esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        s_gatt_if = gatt_if;
        esp_ble_gap_set_device_name(s_device_name);
        esp_ble_gap_config_adv_data(&adv_data);
        s_adv_config_done |= ADV_DATA_FLAG;
        esp_ble_gap_config_adv_data(&scan_rsp_data);
        s_adv_config_done |= RSP_DATA_FLAG;
        esp_ble_gatts_create_attr_tab(nus_attr_table, gatt_if, ATTR_IDX_COUNT, 0);
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status == ESP_GATT_OK &&
            param->add_attr_tab.num_handle == ATTR_IDX_COUNT)
        {
            memcpy(attr_handle_table, param->add_attr_tab.handles, sizeof(attr_handle_table));
            esp_ble_gatts_start_service(attr_handle_table[ATTR_IDX_SERVICE]);
            ESP_LOGI(TAG, "NUS service started");
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        s_conn_id = param->connect.conn_id;
        s_is_connected = true;
        memcpy(s_remote_bda, param->connect.remote_bda, 6);
        ESP_LOGI(TAG, "Connected id=%d mac=%02X:%02X:%02X:%02X:%02X:%02X",
                 s_conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1],
                 param->connect.remote_bda[2], param->connect.remote_bda[3],
                 param->connect.remote_bda[4], param->connect.remote_bda[5]);
        {
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, 6);
            conn_params.min_int = 0x10;
            conn_params.max_int = 0x20;
            conn_params.latency = 0;
            conn_params.timeout = 1000;
            esp_ble_gap_update_conn_params(&conn_params);
        }
        idle_timer_start();
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "Disconnected reason=0x%x", param->disconnect.reason);
        idle_timer_stop();
        s_is_connected = false;
        s_notify_enabled = false;
        s_conn_id = 0xFFFF;
        s_negotiated_mtu = 23;
        /*
         * 断开后重新启动广播:
         * 先调 stop 确保广播处于已停止状态, 在 ADV_STOP_COMPLETE 事件中再 start.
         * 如果 stop 返回失败(广播已停止), 则直接 start.
         */
        s_pending_restart_adv = true;
        if (esp_ble_gap_stop_advertising() != ESP_OK)
        {
            s_pending_restart_adv = false;
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;

    case ESP_GATTS_WRITE_EVT:
        if (!param->write.is_prep)
        {
            if (param->write.handle == attr_handle_table[ATTR_IDX_RX_CHAR_VAL])
            {
                ble_uart_activity_reset();
                ble_cmd_on_data_received(param->write.value, param->write.len);
            }
            else if (param->write.handle == attr_handle_table[ATTR_IDX_TX_CHAR_CCC] &&
                     param->write.len == 2)
            {
                uint16_t ccc_value = param->write.value[0] | (param->write.value[1] << 8);
                s_notify_enabled = (ccc_value == 0x0001);
                ESP_LOGI(TAG, "Notify %s", s_notify_enabled ? "ON" : "OFF");
            }
        }
        if (param->write.need_rsp)
        {
            esp_ble_gatts_send_response(gatt_if, param->write.conn_id,
                                        param->write.trans_id, ESP_GATT_OK, NULL);
        }
        break;

    case ESP_GATTS_MTU_EVT:
        s_negotiated_mtu = param->mtu.mtu;
        ESP_LOGI(TAG, "MTU negotiated: %d (payload max: %d)", s_negotiated_mtu, s_negotiated_mtu - 3);
        break;

    default:
        break;
    }
}

/* ============================================================ */
/* 公共 API                                                       */
/* ============================================================ */

bool ble_uart_is_connected(void)
{
    return s_is_connected;
}

uint16_t ble_uart_get_mtu(void)
{
    return s_negotiated_mtu;
}

int ble_uart_send(const uint8_t *data, uint16_t len)
{
    if (!s_is_connected || !s_notify_enabled || s_gatt_if == ESP_GATT_IF_NONE)
    {
        return -1;
    }

    /* ATT payload = MTU - 3 (ATT header) */
    uint16_t chunk_max = s_negotiated_mtu - 3;
    if (chunk_max < 20) chunk_max = 20;   /* 最小 BLE 4.0 payload */

    uint16_t offset = 0;
    while (offset < len)
    {
        uint16_t chunk = (len - offset > chunk_max) ? chunk_max : (len - offset);
        esp_err_t ret = esp_ble_gatts_send_indicate(
            s_gatt_if, s_conn_id, attr_handle_table[ATTR_IDX_TX_CHAR_VAL],
            chunk, (uint8_t *)(data + offset), false);
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "send_indicate fail at offset %d, err=0x%x", offset, ret);
            return -2;
        }
        offset += chunk;

        /* 多片时需要等 BLE 协议栈完成上一次发送, 否则缓冲区溢出 */
        if (offset < len)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    return 0;
}

void ble_uart_init(void)
{
    /* 设备名: CTM=XXXXXXXXXXXX */
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(s_device_name, sizeof(s_device_name), "CTM=%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "BLE name: %s", s_device_name);

    /* 创建空闲断连定时器 */
    esp_timer_create_args_t timer_args = {
        .callback = idle_timer_callback,
        .arg = s_remote_bda,
        .name = "ble_idle",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_idle_timer));

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_config));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(NUS_APP_ID));
    esp_ble_gatt_set_local_mtu(BLE_MTU_MAX);

    ESP_LOGI(TAG, "BLE UART init OK (MTU max=%d)", BLE_MTU_MAX);
}