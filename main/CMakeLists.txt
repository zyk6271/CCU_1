if(${IDF_TARGET} STREQUAL "linux")
    set(requires esp_stubs protocol_examples_common nvs_flash storage button led heater_uart key)
endif()
idf_component_register(SRCS "main.c" "ota.c"
                    INCLUDE_DIRS "."
)