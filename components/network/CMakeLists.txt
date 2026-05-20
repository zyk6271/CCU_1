idf_component_register(SRCS "ble_uart.c" "ble_cmd_handler.c" "network_service.c" "wifi_api.c" "system.c" "mcu_api.c" "wifi_manager.c" "wifi_service.c" "iot_event.c" "tcp_service.c" "mqtt_service.c"
                    INCLUDE_DIRS "include"
                    REQUIRES mqtt crypto esp_netif bt app_update esp_timer esp_wifi led wpa_supplicant nvs_flash storage freertos heater_uart)
