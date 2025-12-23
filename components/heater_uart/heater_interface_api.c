
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
#include "esp_log.h"

uint8_t device_type = HEATER_TYPE_NONE;

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

uint8_t heater_interface_burn_status_read(void)
{
    uint8_t value = 0;
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            value = heater_noritz_burn_status_read();
            break;
        case HEATER_TYPE_RINNAI_HOME:
            value = heater_rinnai_burn_status_read();
            break;
        case HEATER_TYPE_RINNAL_BUSINESS:
            value = heater_rinnai_bussiness_burn_status_read();
            break;
        default:
            break;
    }

    return value;
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

uint8_t heater_interface_temperature_value_read(void)
{
    uint8_t value = 0;
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            value = heater_noritz_temp_read();
            break;
        case HEATER_TYPE_RINNAI_HOME:
            value = heater_rinnai_temp_read();
            break;
        case HEATER_TYPE_RINNAL_BUSINESS:
            value = heater_rinnai_bussiness_temp_read();
            break;
        default:
            break;
    }

    return value;
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

uint8_t heater_interface_eco_read(void)
{
    uint8_t value = 0;
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            value = heater_noritz_eco_read();
            break;
        case HEATER_TYPE_RINNAI_HOME:
            value = heater_rinnai_eco_read();
            break;
        default:
            break;
    }

    return value;
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

uint8_t heater_interface_circulation_value_read(void)
{
    uint8_t value = 0;
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            value = heater_noritz_circle_read();
            break;
        case HEATER_TYPE_RINNAI_HOME:
            value = heater_rinnai_circle_read();
            break;
        case HEATER_TYPE_RINNAL_BUSINESS:
            value = heater_rinnai_bussiness_circle_read();
            break;
        default:
            break;
    }

    return value;
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

uint8_t heater_interface_power_value_read(void)
{
    uint8_t value = 0;
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            value = heater_noritz_onoff_read();
            break;
        case HEATER_TYPE_RINNAI_HOME:
            value = heater_rinnai_onoff_read();
            break;
        case HEATER_TYPE_RINNAL_BUSINESS:
            value = heater_rinnai_bussiness_onoff_read();
            break;
        default:
            break;
    }

    return value;
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

uint8_t heater_interface_priority_read(void)
{
    uint8_t value = 0;
    switch(device_type)
    {
        case HEATER_TYPE_NORITZ_HOME:
            value = heater_noritz_priority_read();
            break;
        case HEATER_TYPE_RINNAI_HOME:
            value = heater_rinnai_priority_read();
            break;
        default:
            break;
    }

    return value;
}
