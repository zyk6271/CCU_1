#include "stdint.h"

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
    uint8_t error_record[9];
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
}heater_rinnai_bussiness_info_t;

uint8_t heater_rinnai_bussiness_data_length_find(uint64_t command);
void heater_rinnai_bussiness_poll_callback(void);
void heater_rinnai_bussiness_poll_status_resset(void);
void heater_rinnai_bussiness_info_read(void);
void heater_rinnai_bussiness_error_read(void);
void heater_rinnai_bussiness_total_flow_rate_read(void);
void heater_rinnai_bussiness_fan_speed_read(void);
void heater_rinnai_bussiness_poweron_time_read(void);
void heater_rinnai_bussiness_combustion_read(void);
void heater_rinnai_bussiness_combustion_times_read(void);
void heater_rinnai_bussiness_combustion_status_read(void);
void heater_rinnai_bussiness_error_record_read(void);
void heater_rinnai_bussiness_current_temp_read(void);
void heater_rinnai_bussiness_onoff_setting_read(void);
void wifi_rinnai_bussiness_command_info_upload(void);
void heater_rinnai_bussiness_data_handle(uint8_t offset);