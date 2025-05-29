
#include "wifi_api.h"
#include "network_typedef.h"
#include "crypto_aes.h"
#include "esp_timer.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "heater_rinnai_api.h"
#include "heater_rinnai_bussiness_api.h"
#include "heater_noritz_api.h"
#include "heater_interface_api.h"
#include "dishwasher_modbus_api.h"
#include "esp_log.h"

uint8_t device_type = MODBUS_TYPE_DISHWASHER;

void hearter_device_type_set(uint8_t type)
{
    device_type = type;
}

uint8_t hearter_device_type_get(void)
{
    return device_type;
}

void heater_interface_status_reset(void)
{
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            heater_noritz_poll_status_resset();
            break;
        case HEATER_TYPE_RINNAI_HOME:
            heater_rinnai_poll_status_resset();
            break;
        case HEATER_TYPE_RINNAL_BUSINESS:
            heater_rinnai_bussiness_poll_status_resset();
            break;
        case MODBUS_TYPE_DISHWASHER:
            dishwasher_poll_status_reset();
            break;
        default:
            break;
    }
}

void heater_interface_error_read(void)
{
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            heater_noritz_error_read();
            break;
        case HEATER_TYPE_RINNAI_HOME:
            heater_rinnai_error_read();
            break;
        default:
            break;
    }
}

void heater_interface_info_read(void)
{
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            heater_noritz_info_read();
            break;
        case HEATER_TYPE_RINNAI_HOME:
            heater_rinnai_info_read();
            break;
        default:
            break;
    }
}

void heater_interface_temperature_setting(uint8_t value)
{
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            heater_noritz_temperature_write(value);
            break;
        case HEATER_TYPE_RINNAI_HOME:
            heater_rinnai_temperature_write(value);
            break;
        default:
            break;
    }
}

void heater_interface_eco_setting(uint8_t value)
{
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            heater_noritz_eco_write(value);
            break;
        case HEATER_TYPE_RINNAI_HOME:
            heater_rinnai_eco_write(value);
            break;
        default:
            break;
    }
}

void heater_interface_circulation_setting(uint8_t value)
{
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            heater_noritz_circulation_write(value);
            break;
        case HEATER_TYPE_RINNAI_HOME:
            heater_rinnai_circulation_write(value);
            break;
        default:
            break;
    }
}

void heater_interface_power_setting(uint8_t value)
{
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            heater_noritz_power_write(value);
            break;
        case HEATER_TYPE_RINNAI_HOME:
            heater_rinnai_power_write(value);
            break;
        default:
            break;
    }
}

void heater_interface_priority_setting(uint8_t value)
{
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            heater_noritz_priority_write(value);
            break;
        case HEATER_TYPE_RINNAI_HOME:
            heater_rinnai_priority_write(value);
            break;
        default:
            break;
    }
}
