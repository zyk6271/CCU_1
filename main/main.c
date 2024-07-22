#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "wifi_service.h"
#include "storage.h"
#include "crypto_aes.h"
#include "signal_led.h"
#include "wifi_manager.h"
#include "key.h"
#include "heater_uart.h"

void app_main(void)
{
    storage_init();
    key_init();
    signal_led_init();
    crypto_initialize();
    wifi_interface_init();
    wifi_service_init();
    heter_uart_init();
}
