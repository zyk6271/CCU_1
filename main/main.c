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
#include "heater_uart.h"
#include "dishwasher_modbus_api.h"

static const char *TAG = "main";

const uint8_t fw_main_ver = 0x01;
const uint8_t fw_sub_ver = 0x04;

void app_main(void)
{
    ESP_LOGI(TAG,"System Version is V1.%d.%d",fw_main_ver,fw_sub_ver);
    storage_init();
    key_init();
    signal_led_init();
    crypto_initialize();
    wifi_interface_init();
    wifi_service_init();
    dish_washer_modbus_init();
    //heter_uart_init();
}
