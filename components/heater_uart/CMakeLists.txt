idf_component_register(SRCS "ccu_modbus_api.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver network esp_timer esp-modbus)
