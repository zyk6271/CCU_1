#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_mac.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "errno.h"

#include "led.h"
#include "ota.h"

#define GATTS_TABLE_TAG             "OTA_SERVER"
#define PROFILE_NUM                 2  // 改为2，一个用于OTA服务，一个用于设备信息服务
#define PROFILE_OTA_APP_IDX         0
#define PROFILE_DEVICE_INFO_APP_IDX 1
#define OTA_APP_ID                  0x55
#define DEVICE_INFO_APP_ID          0x56
#define SAMPLE_DEVICE_NAME          "TG_CTM"
#define SVC_INST_ID                 0

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 500
#define PREPARE_BUF_MAX_SIZE        1024
#define CHAR_DECLARATION_SIZE       (sizeof(uint8_t))

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

// OTA服务索引
enum
{
    OTA_IDX_SVC,
    OTA_IDX_CHAR_A,
    OTA_IDX_CHAR_VAL_A,
    OTA_IDX_CHAR_B,
    OTA_IDX_CHAR_VAL_B,
    OTA_IDX_NB,
};

// 设备信息服务索引
enum
{
    DEV_INFO_IDX_SVC,
    DEV_INFO_IDX_MANUFACTURER_NAME_CHAR,
    DEV_INFO_IDX_MANUFACTURER_NAME_VAL,
    DEV_INFO_IDX_MODEL_NUMBER_CHAR,
    DEV_INFO_IDX_MODEL_NUMBER_VAL,
    DEV_INFO_IDX_FIRMWARE_REV_CHAR,
    DEV_INFO_IDX_FIRMWARE_REV_VAL,
    DEV_INFO_IDX_SERIAL_NUMBER_CHAR,
    DEV_INFO_IDX_SERIAL_NUMBER_VAL,
    DEV_INFO_IDX_NB,
};

static uint8_t adv_config_done = 0;

uint16_t ota_handle_table[OTA_IDX_NB];
uint16_t device_info_handle_table[DEV_INFO_IDX_NB];

typedef struct {
    uint8_t                 *prepare_buf;
    int                     prepare_len;
} prepare_type_env_t;

static prepare_type_env_t prepare_write_env;

esp_err_t err;
esp_ota_handle_t update_handle = 0 ;
const esp_partition_t *update_partition = NULL;

/*
Type: com.silabs.service.ota
UUID: 1D14D6EE-FD63-4FA1-BFA4-8F47B42119F0
*/
static uint8_t service_uuid[16] = {
    0xf0, 0x19, 0x21, 0xb4, 0x47, 0x8f, 0xa4, 0xbf, 0xa1, 0x4f, 0x63, 0xfd, 0xee, 0xd6, 0x14, 0x1d,
};

/*
Type: com.silabs.characteristic.ota_control
UUID: F7BF3564-FB6D-4E53-88A4-5E37E0326063
Silicon Labs OTA Control.
Property requirements: 
	Notify - Excluded
	Read - Excluded
	Write Without Response - Excluded
	Write - Mandatory
	Reliable write - Excluded
	Indicate - Excluded
*/
static uint8_t char_ota_control_uuid[16] = {
    0x63, 0x60, 0x32, 0xe0, 0x37, 0x5e, 0xa4, 0x88, 0x53, 0x4e, 0x6d, 0xfb, 0x64, 0x35, 0xbf, 0xf7,
};

/*
Type: com.silabs.characteristic.ota_data
UUID: 984227F3-34FC-4045-A5D0-2C581F81A153
Silicon Labs OTA Data.
Property requirements: 
	Notify - Excluded
	Read - Excluded
	Write Without Response - Mandatory
	Write - Mandatory
	Reliable write - Excluded
	Indicate - Excluded
*/
static uint8_t char_ota_data_uuid[16] = {
    0x53, 0xa1, 0x81, 0x1f, 0x58, 0x2c, 0xd0, 0xa5, 0x45, 0x40, 0xfc, 0x34, 0xf3, 0x27, 0x42, 0x98,
};

// Device Information Service UUID (标准蓝牙 UUID: 0x180A)
static const uint16_t device_info_service_uuid = 0x180A;

// 特征 UUID (标准蓝牙 UUID)
static const uint16_t manufacturer_name_uuid = 0x2A29;      // 制造商名称
static const uint16_t model_number_uuid = 0x2A24;          // 型号
static const uint16_t firmware_rev_uuid = 0x2A26;          // 固件版本
static const uint16_t serial_number_uuid = 0x2A25;         // 序列号

// 设备信息值
static const char manufacturer_name[] = "Sentient";
static const char model_number[] = "CI-CTM2B-01";
extern const uint8_t firmware_rev_val[6];
static char serial_number[16] = {0};

/* The length of adv data must be less than 31 bytes */
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval        = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance          = 0x00,
    .manufacturer_len    = 0,    //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //test_manufacturer,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(service_uuid),
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// scan response data
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp        = true,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x00,
    .manufacturer_len    = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //&test_manufacturer[0],
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(service_uuid),
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
					esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst profile_tab[PROFILE_NUM] = {
    [PROFILE_OTA_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
        .app_id = OTA_APP_ID,
    },
    [PROFILE_DEVICE_INFO_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
        .app_id = DEVICE_INFO_APP_ID,
    },
};

static const uint16_t primary_service_uuid         = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint8_t char_value[4]                 = {0x11, 0x22, 0x33, 0x44};
static const uint8_t char_prop_write_writenorsp    =  ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
// Device Info 特征属性（只读）
static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;

/* OTA Service GATT Database */
static const esp_gatts_attr_db_t ota_gatt_db[OTA_IDX_NB] =
{
    // OTA Service Declaration
    [OTA_IDX_SVC]        =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(service_uuid), sizeof(service_uuid), (uint8_t *)&service_uuid}},

    /* OTA Control Characteristic Declaration */
    [OTA_IDX_CHAR_A]     =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write_writenorsp}},

    /* OTA Control Characteristic Value */
    [OTA_IDX_CHAR_VAL_A] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)&char_ota_control_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},

    /* OTA Data Characteristic Declaration */
    [OTA_IDX_CHAR_B]      =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write_writenorsp}},

    /* OTA Data Characteristic Value */
    [OTA_IDX_CHAR_VAL_B]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)&char_ota_data_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},
};

/* Device Information Service GATT Database */
static const esp_gatts_attr_db_t device_info_gatt_db[DEV_INFO_IDX_NB] =
{
    // Device Information Service Declaration
    [DEV_INFO_IDX_SVC] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(device_info_service_uuid), sizeof(device_info_service_uuid), (uint8_t *)&device_info_service_uuid}},

    /* Manufacturer Name Characteristic Declaration */
    [DEV_INFO_IDX_MANUFACTURER_NAME_CHAR] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read}},

    /* Manufacturer Name Characteristic Value */
    [DEV_INFO_IDX_MANUFACTURER_NAME_VAL] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&manufacturer_name_uuid, ESP_GATT_PERM_READ,
      strlen(manufacturer_name), strlen(manufacturer_name), (uint8_t *)manufacturer_name}},

    /* Model Number Characteristic Declaration */
    [DEV_INFO_IDX_MODEL_NUMBER_CHAR] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read}},

    /* Model Number Characteristic Value */
    [DEV_INFO_IDX_MODEL_NUMBER_VAL] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&model_number_uuid, ESP_GATT_PERM_READ,
      strlen(model_number), strlen(model_number), (uint8_t *)model_number}},

    /* Firmware Revision Characteristic Declaration */
    [DEV_INFO_IDX_FIRMWARE_REV_CHAR] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read}},

    /* Firmware Revision Characteristic Value */
    [DEV_INFO_IDX_FIRMWARE_REV_VAL] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&firmware_rev_uuid, ESP_GATT_PERM_READ,
        sizeof(firmware_rev_val), sizeof(firmware_rev_val), (uint8_t *)firmware_rev_val}},

    /* Serial Number Characteristic Declaration */
    [DEV_INFO_IDX_SERIAL_NUMBER_CHAR] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read}},

    /* Serial Number Characteristic Value */
    [DEV_INFO_IDX_SERIAL_NUMBER_VAL] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&serial_number_uuid, ESP_GATT_PERM_READ,
      sizeof(serial_number), sizeof(serial_number), (uint8_t *)serial_number}},
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~ADV_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            /* advertising start complete event to indicate advertising start successfully or failed */
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TABLE_TAG, "advertising start failed");
            }else{
                ESP_LOGI(GATTS_TABLE_TAG, "advertising start successfully");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TABLE_TAG, "Advertising stop failed");
            }
            else {
                ESP_LOGI(GATTS_TABLE_TAG, "Stop adv successfully\n");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
            break;
        default:
            break;
    }
}

void example_prepare_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGI(GATTS_TABLE_TAG, "prepare write, handle = %d, value len = %d", param->write.handle, param->write.len);
    esp_gatt_status_t status = ESP_GATT_OK;
    if (prepare_write_env->prepare_buf == NULL) {
        prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
        prepare_write_env->prepare_len = 0;
        if (prepare_write_env->prepare_buf == NULL) {
            ESP_LOGE(GATTS_TABLE_TAG, "%s, Gatt_server prep no mem", __func__);
            status = ESP_GATT_NO_RESOURCES;
        }
    } else {
        if(param->write.offset > PREPARE_BUF_MAX_SIZE) {
            status = ESP_GATT_INVALID_OFFSET;
        } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
            status = ESP_GATT_INVALID_ATTR_LEN;
        }
    }
    /*send response when param->write.need_rsp is true */
    if (param->write.need_rsp){
        esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
        if (gatt_rsp != NULL){
            gatt_rsp->attr_value.len = param->write.len;
            gatt_rsp->attr_value.handle = param->write.handle;
            gatt_rsp->attr_value.offset = param->write.offset;
            gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
            memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
            esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
            if (response_err != ESP_OK){
               ESP_LOGE(GATTS_TABLE_TAG, "Send response error");
            }
            free(gatt_rsp);
        }else{
            ESP_LOGE(GATTS_TABLE_TAG, "%s, malloc failed", __func__);
        }
    }
    if (status != ESP_GATT_OK){
        return;
    }
    memcpy(prepare_write_env->prepare_buf + param->write.offset,
           param->write.value,
           param->write.len);
    prepare_write_env->prepare_len += param->write.len;

}

void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param){
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC && prepare_write_env->prepare_buf){
        ESP_LOG_BUFFER_HEX(GATTS_TABLE_TAG, prepare_write_env->prepare_buf, prepare_write_env->prepare_len);
    }else{
        ESP_LOGI(GATTS_TABLE_TAG,"ESP_GATT_PREP_WRITE_CANCEL");
    }
    if (prepare_write_env->prepare_buf) {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    uint8_t profile_idx = 0;
    
    // 确定是哪个profile的事件
    for (int i = 0; i < PROFILE_NUM; i++) {
        if (gatts_if == ESP_GATT_IF_NONE || gatts_if == profile_tab[i].gatts_if) {
            if (profile_tab[i].gatts_cb) {
                profile_idx = i;
                break;
            }
        }
    }
    
    switch (event) {
        case ESP_GATTS_REG_EVT:{
            ESP_LOGI(GATTS_TABLE_TAG, "REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
            profile_tab[profile_idx].service_id.is_primary = true;
            profile_tab[profile_idx].service_id.id.inst_id = 0x00;
            profile_tab[profile_idx].service_id.id.uuid.len = ESP_UUID_LEN_128;
            
            if (profile_idx == PROFILE_OTA_APP_IDX) {
                // OTA服务
                memcpy(profile_tab[profile_idx].service_id.id.uuid.uuid.uuid128,service_uuid,sizeof(profile_tab[profile_idx].service_id.id.uuid.uuid.uuid128));
                esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(ota_gatt_db, gatts_if, OTA_IDX_NB, 0);
                if (create_attr_ret){
                    ESP_LOGE(GATTS_TABLE_TAG, "create OTA attr table failed, error code = %x", create_attr_ret);
                }
            } else if (profile_idx == PROFILE_DEVICE_INFO_APP_IDX) {
                // 设备信息服务
                profile_tab[profile_idx].service_id.id.uuid.uuid.uuid16 = device_info_service_uuid;
                profile_tab[profile_idx].service_id.id.uuid.len = ESP_UUID_LEN_16;
                esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(device_info_gatt_db, gatts_if, DEV_INFO_IDX_NB, 0);
                if (create_attr_ret){
                    ESP_LOGE(GATTS_TABLE_TAG, "create device info attr table failed, error code = %x", create_attr_ret);
                }
            }
            
            // 只在OTA服务注册时设置广播数据
            if (profile_idx == PROFILE_OTA_APP_IDX) {
                esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(SAMPLE_DEVICE_NAME);
                if (set_dev_name_ret){
                    ESP_LOGE(GATTS_TABLE_TAG, "set device name failed, error code = %x", set_dev_name_ret);
                }
                //config adv data
                esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
                if (ret){
                    ESP_LOGE(GATTS_TABLE_TAG, "config adv data failed, error code = %x", ret);
                }
                adv_config_done |= ADV_CONFIG_FLAG;
                //config scan response data
                ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
                if (ret){
                    ESP_LOGE(GATTS_TABLE_TAG, "config scan response data failed, error code = %x", ret);
                }
                adv_config_done |= SCAN_RSP_CONFIG_FLAG;
            }
        }
       	    break;
        case ESP_GATTS_READ_EVT:
       	    break;
        case ESP_GATTS_WRITE_EVT:
            if (!param->write.is_prep){
                if (profile_idx == PROFILE_OTA_APP_IDX) {
                    // OTA服务的写事件处理
                    if (ota_handle_table[OTA_IDX_CHAR_VAL_A] == param->write.handle && param->write.len == 1){
                        uint8_t value = param->write.value[0];
                        ESP_LOGI(GATTS_TABLE_TAG, "ota-control = %d",value);
                        if(0x00 == value){
                            ESP_LOGI(GATTS_TABLE_TAG, "======beginota======");
                            update_partition = esp_ota_get_next_update_partition(NULL);
                            ESP_LOGI(GATTS_TABLE_TAG, "Writing to partition subtype %d at offset 0x%x",
                                    update_partition->subtype, update_partition->address);
                            assert(update_partition != NULL);
                            err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                            if (err != ESP_OK) {
                                ESP_LOGE(GATTS_TABLE_TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                                esp_ota_abort(update_handle);
                            }
                        }
                        else if(0x03 == value){
                            ESP_LOGI(GATTS_TABLE_TAG, "======endota======");
                            err = esp_ota_end(update_handle);
                            if (err != ESP_OK) {
                                if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
                                    ESP_LOGE(GATTS_TABLE_TAG, "Image validation failed, image is corrupted");
                                }
                                ESP_LOGE(GATTS_TABLE_TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
                            }

                            err = esp_ota_set_boot_partition(update_partition);
                            if (err != ESP_OK) {
                                ESP_LOGE(GATTS_TABLE_TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
                            }
                            ESP_LOGI(GATTS_TABLE_TAG, "Prepare to restart system!");
                            esp_restart();
                            return ;
                        }
                    }
                    if (ota_handle_table[OTA_IDX_CHAR_VAL_B] == param->write.handle){
                        uint16_t length = param->write.len;
                        err = esp_ota_write( update_handle, (const void *)param->write.value, length);
                        if (err != ESP_OK) {
                            esp_ota_abort(update_handle);
                            ESP_LOGI(GATTS_TABLE_TAG, "esp_ota_write error!");
                        }
                    }
                }

                /* send response when param->write.need_rsp is true*/
                if (param->write.need_rsp){
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                }
            }else{
                /* handle prepare write */
                example_prepare_write_event_env(gatts_if, &prepare_write_env, param);
            }
      	    break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_EXEC_WRITE_EVT");
            example_exec_write_event_env(&prepare_write_env, param);
            break;
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            break;
        case ESP_GATTS_CONF_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_CONF_EVT, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
            break;
        case ESP_GATTS_START_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
            break;
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
            ESP_LOG_BUFFER_HEX(GATTS_TABLE_TAG, param->connect.remote_bda, 6);
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            conn_params.latency = 0;
            conn_params.max_int = 0x20;
            conn_params.min_int = 0x10;
            conn_params.timeout = 1000;
            esp_ble_gap_update_conn_params(&conn_params);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
            if (param->add_attr_tab.status != ESP_GATT_OK){
                ESP_LOGE(GATTS_TABLE_TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            else {
                ESP_LOGI(GATTS_TABLE_TAG, "create attribute table successfully, the number handle = %d, app_id = %d\n",
                        param->add_attr_tab.num_handle, profile_tab[profile_idx].app_id);
                
                if (profile_idx == PROFILE_OTA_APP_IDX) {
                    if (param->add_attr_tab.num_handle != OTA_IDX_NB){
                        ESP_LOGE(GATTS_TABLE_TAG, "create OTA attribute table abnormally, num_handle (%d) doesn't equal to OTA_IDX_NB(%d)", 
                                param->add_attr_tab.num_handle, OTA_IDX_NB);
                    } else {
                        memcpy(ota_handle_table, param->add_attr_tab.handles, sizeof(ota_handle_table));
                        esp_ble_gatts_start_service(ota_handle_table[OTA_IDX_SVC]);
                        ESP_LOGI(GATTS_TABLE_TAG, "OTA service started");
                    }
                } else if (profile_idx == PROFILE_DEVICE_INFO_APP_IDX) {
                    if (param->add_attr_tab.num_handle != DEV_INFO_IDX_NB){
                        ESP_LOGE(GATTS_TABLE_TAG, "create device info attribute table abnormally, num_handle (%d) doesn't equal to DEV_INFO_IDX_NB(%d)", 
                                param->add_attr_tab.num_handle, DEV_INFO_IDX_NB);
                    } else {
                        memcpy(device_info_handle_table, param->add_attr_tab.handles, sizeof(device_info_handle_table));
                        esp_ble_gatts_start_service(device_info_handle_table[DEV_INFO_IDX_SVC]);
                        ESP_LOGI(GATTS_TABLE_TAG, "Device Info service started");
                    }
                }
            }
            break;
        }
        case ESP_GATTS_STOP_EVT:
        case ESP_GATTS_OPEN_EVT:
        case ESP_GATTS_CANCEL_OPEN_EVT:
        case ESP_GATTS_CLOSE_EVT:
        case ESP_GATTS_LISTEN_EVT:
        case ESP_GATTS_CONGEST_EVT:
        case ESP_GATTS_UNREG_EVT:
        case ESP_GATTS_DELETE_EVT:
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        for (int idx = 0; idx < PROFILE_NUM; idx++) {
            if (param->reg.app_id == profile_tab[idx].app_id) {
                profile_tab[idx].gatts_if = gatts_if;
                ESP_LOGI(GATTS_TABLE_TAG, "Profile %d registered, gatts_if = %d", idx, gatts_if);
                break;
            }
        }
    }
    
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == profile_tab[idx].gatts_if) {
                if (profile_tab[idx].gatts_cb) {
                    profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

void ota_init(void)
{
    esp_err_t ret;

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    unsigned char mac_base[6] = {0};
    esp_read_mac(mac_base, ESP_MAC_BT);
    sprintf((char *)serial_number,"%02x%02x%02x%02x%02x%02x",mac_base[0],mac_base[1],mac_base[2],mac_base[3],mac_base[4],mac_base[5]);

    printf("mac_addr:%02X:%02X:%02X:%02X:%02X:%02X\r\n", mac_base[0],mac_base[1],mac_base[2],mac_base[3],mac_base[4],mac_base[5]);
    printf("mac_str:%s\r\n",serial_number);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(GATTS_TABLE_TAG, "gatts register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(GATTS_TABLE_TAG, "gap register error, error code = %x", ret);
        return;
    }

    // 注册OTA服务
    ret = esp_ble_gatts_app_register(OTA_APP_ID);
    if (ret){
        ESP_LOGE(GATTS_TABLE_TAG, "gatts app register error for OTA, error code = %x", ret);
        return;
    }

    // 注册设备信息服务
    ret = esp_ble_gatts_app_register(DEVICE_INFO_APP_ID);
    if (ret){
        ESP_LOGE(GATTS_TABLE_TAG, "gatts app register error for Device Info, error code = %x", ret);
        return;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret){
        ESP_LOGE(GATTS_TABLE_TAG, "set local MTU failed, error code = %x", local_mtu_ret);
    }

    led_network_status_handle(4);
}