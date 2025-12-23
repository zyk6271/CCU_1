#include "stdint.h"

// #define HEATER_INTERFACE_TYPE   1   //UART
#define HEATER_INTERFACE_TYPE   2   //MODBUS

#define HEATER_HEART_ENABLED    1
#define HEATER_HEART_PERIOD    30 //30 is normal,300 is modbus without difference

enum heater_type
{
    HEATER_TYPE_HOME,
    HEATER_TYPE_NORITZ_HOME,
    HEATER_TYPE_RINNAI_HOME,
    HEATER_TYPE_RINNAL_BUSINESS,
    MODBUS_TYPE_DISHWASHER,
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