#include "stdint.h"

typedef struct
{
    uint8_t error[2];
    uint8_t total_flow_rate[3];
    uint8_t flow_rate_of_heat_exchanger[3];
    uint8_t flow_rate_of_circulation_pump [3];
    uint8_t fan_speed[3];
    uint8_t power_on_time[5];
    uint8_t combustion_time[5];
    uint8_t combustion_number_times[5];
    uint8_t input_water_thermistor_detection_temperature[2];
    uint8_t inlet_water_thermistor_detection_temperature[2];
    uint8_t heat_exchanger_temperature[2];
    uint8_t error_record[16];
    uint8_t gas_overflow;
    uint8_t gas_consumption[5];
    uint8_t hot_water_overflow;
    uint8_t hot_water_consumption[5];
    uint8_t combustion_status[2];
    uint8_t current_priority_location[2];
    uint8_t current_temperature_setting[2];
    uint8_t eco_status[2];
    uint8_t circulation_status[2];
    uint8_t on_off_setting[2];
}noritz_state_info_frame_t;

typedef struct
{
    uint8_t type;
    uint8_t model;
    uint8_t error;
    uint32_t total_flow_rate;
    uint32_t flow_rate_of_heat_exchanger;
    uint32_t flow_rate_of_circulation_pump;
    uint32_t fan_speed;
    uint32_t power_on_time;
    uint32_t combustion_time;
    uint32_t combustion_number_times;
    uint8_t input_water_thermistor_detection_temperature;
    uint8_t inlet_water_thermistor_detection_temperature;
    uint8_t heat_exchanger_temperature;
    uint8_t error_record[8];
    uint8_t gas_overflow;
    uint32_t gas_consumption;
    uint8_t hot_water_overflow;
    uint32_t hot_water_consumption;
    uint8_t combustion_status;
    uint8_t current_priority_location;
    uint8_t current_temperature_setting;
    uint8_t eco_status;
    uint8_t circulation_status;
    uint8_t on_off_setting;
}noritz_heater_info_t;

void heater_noritz_poll_status_resset(void);
void heater_noritz_info_read(void);
void heater_noritz_error_read(void);
void heater_noritz_temperature_write(uint8_t value);
void heater_noritz_eco_write(uint8_t value);
void heater_noritz_circulation_write(uint8_t value);
void heater_noritz_power_write(uint8_t value);
void heater_noritz_priority_write(uint8_t value);
void heater_noritz_data_handle(uint8_t offset);