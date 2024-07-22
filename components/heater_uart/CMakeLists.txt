idf_component_register(SRCS "heater_noritz_api.c" "heater_rinnai_api.c" "heater_uart_interface.c" "heater_uart_service.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver network esp_timer)
