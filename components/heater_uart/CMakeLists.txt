idf_component_register(SRCS "heater_rinnai_bussiness_api.c" "heater_interface_api.c" "heater_noritz_api.c" "heater_rinnai_api.c" "heater_uart_interface.c" "heater_uart_service.c" "dishwasher_modbus_api.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver network esp_timer esp-modbus)
