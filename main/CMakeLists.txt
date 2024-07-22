if(${IDF_TARGET} STREQUAL "linux")
    set(requires esp_stubs protocol_examples_common nvs_flash storage button led heater_uart)
endif()
idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
)