#include "stdint.h"

enum heater_type
{
    HEATER_TYPE_NONE,
    HEATER_TYPE_NORITZ_HOME,
    HEATER_TYPE_RINNAI_HOME,
    HEATER_TYPE_RINNAL_BUSINESS,
};

void hearter_device_type_set(uint8_t type);
uint8_t hearter_device_type_get(void);
void heater_interface_status_reset(void);
void heater_interface_error_read(void);
void heater_interface_info_read(void);
void heater_interface_temperature_setting(uint8_t value);
void heater_interface_eco_setting(uint8_t value);
void heater_interface_circulation_setting(uint8_t value);
void heater_interface_power_setting(uint8_t value);
void heater_interface_priority_setting(uint8_t value);
uint8_t heater_interface_burn_status_read(void);
uint8_t heater_interface_power_value_read(void);
uint8_t heater_interface_circulation_value_read(void);
uint8_t heater_interface_temperature_value_read(void);
uint8_t heater_interface_priority_read(void);
uint8_t heater_interface_eco_read(void);