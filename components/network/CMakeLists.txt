idf_component_register(SRCS "mcu_api.c" "wifi_manager.c" "system.c" "wifi_api.c" "wifi_service.c" "tcp_client.c" "iot_event.c"
                    INCLUDE_DIRS "include"
                    REQUIRES crypto esp_netif esp_timer esp_wifi led wpa_supplicant nvs_flash storage freertos heater_uart freertos)
