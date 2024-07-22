/*
 * Copyrear (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-08-04     Rick       the first version
 */
#ifndef WIFI_WIFI_API_H_
#define WIFI_WIFI_API_H_

#include "stdint.h"

typedef struct
{
    uint8_t error;
    uint8_t total_flow_rate[2];
    uint8_t flow_rate_of_heat_exchanger[2];
    uint8_t flow_rate_of_circulation_pump [2];
    uint8_t fan_speed;
    uint8_t power_on_time[3];
    uint8_t combustion_time[3];
    uint8_t combustion_number_times[3];
    uint8_t input_water_thermistor_detection_temperature;
    uint8_t inlet_water_thermistor_detection_temperature;
    uint8_t heat_exchanger_temperature;
    uint8_t error_record[8];
    uint8_t gas_overflow;
    uint8_t gas_consumption[2];
    uint8_t hot_water_overflow;
    uint8_t hot_water_consumption[2];
    uint8_t combustion_status;
    uint8_t current_priority_location;
    uint8_t current_temperature_setting;
    uint8_t eco_status;
    uint8_t circulation_status;
    uint8_t on_off_setting;
}rinnai_state_upload_frame_t;

void wifi_poll_status_reset(void);
void wifi_recv_info_upload(void);
void wifi_recv_model_upload(void);
void wifi_recv_temperature_setting(uint8_t value);
void wifi_recv_eco_setting(uint8_t value);
void wifi_recv_circulation_setting(uint8_t value);
void wifi_recv_power_setting(uint8_t value);
void wifi_recv_priority_setting(uint8_t value);
void wifi_heater_common_key_request(void);
void wifi_heater_common_heart_upload(void);
void heater_heart_timer_start(void);
void heater_heart_timer_init(void);
void heater_poll_timer_init(void);
void heater_poll_timer_start(void);
void heater_detect_finish(uint8_t value);
void heater_detect_timer_init(void);

#endif /* WIFI_WIFI_API_H_ */
