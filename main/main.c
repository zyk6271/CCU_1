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
#include "ota.h"
#include "heater_uart.h"
#include "heater_interface_api.h"
#include "ccu_modbus_api.h"

static const char *TAG = "main";

uint8_t firmware_rev_val[6]  = "V1.1.3";

void app_main(void)
{
    ESP_LOGI(TAG,"System Version is %s",firmware_rev_val);
    storage_init();
    key_init();
    signal_led_init();
    if(reset_key_level_get_level() == 0)
    {
        ota_init();
    }
    else
    {
        crypto_initialize();
        wifi_interface_init();
        wifi_service_init();
#if HEATER_INTERFACE_TYPE == 1
        heter_uart_init();
#else
        ccu_modbus_init();
#endif
    }
}
