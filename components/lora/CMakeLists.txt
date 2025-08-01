idf_component_register(SRCS "radio_encoder.c" "radio.c" "radio_protocol.c" "radio_protocol_remote.c" "radio_protocol_utils.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver heater_uart)
