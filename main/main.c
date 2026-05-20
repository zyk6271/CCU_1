/**
 * @file main.c
 * @brief 主入口 (修改: BLE 使用 NUS UART 服务, OTA 也走此通道)
 */
#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "wifi_service.h"
#include "storage.h"
#include "crypto_aes.h"
#include "signal_led.h"
#include "wifi_manager.h"
#include "key.h"
#include "ble_uart.h"
#include "ccu_modbus_api.h"

static const char *TAG = "main";

uint8_t firmware_rev_val[6] = "V1.1.4";

void app_main(void)
{
    ESP_LOGI(TAG, "System Version is %s", firmware_rev_val);
    storage_init();
    key_init();
    signal_led_init();
    ble_uart_init();
    crypto_initialize();
    wifi_interface_init();
    wifi_service_init();
    ccu_modbus_init();
}