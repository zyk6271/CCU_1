idf_component_register(SRCS "key.c" "button.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver freertos led esp_timer network)
