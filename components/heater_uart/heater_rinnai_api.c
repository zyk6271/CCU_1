#include "driver/uart.h"
#include "heater_uart.h"
#include "heater_rinnai_api.h"
#include "esp_log.h"
#include "string.h"
#include "crypto_aes.h"
#include "network_typedef.h"
#include "wifi_api.h"
#include "esp_timer.h"
#include "wifi_manager.h"
#include "heater_interface_api.h"

static const char *TAG = "heater_rinnai";

extern uint32_t send_counter;

uint16_t last_combustion_status = 1;

heater_rinnai_info_t heater_rinnai_info = 
{
    .current_priority_location = 0x02,
};
heater_rinnai_info_t heater_rinnai_info_last = {0};

esp_timer_handle_t rinnai_priority_timer;

uint8_t heater_rinnai_data_length_find(uint16_t command)
{
    uint8_t length = 0;
    switch(command)
    {
        case 0x2020:
            length = 4;
            break;
        case 0x2030:
            length = 88;
            break;
        case 0x3021:
        case 0x3022:
        case 0x3023:
        case 0x3024:
        case 0x3025:
            length = 2;
            break;
        default:
            break;
    }

    return length;
}

void heater_rinnai_poll_status_resset(void)
{
    memset(&heater_rinnai_info_last,0,sizeof(heater_rinnai_info_t));
}

void heater_rinnai_info_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFB);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x20);
    send_len = set_heater_uart_tx_byte(send_len,0x20);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_error_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFB);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x20);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_temperature_write(uint8_t value)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFB);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x21);
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value >> 4));
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value & 0xF));
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_eco_write(uint8_t value)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFB);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x22);
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value >> 4));
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value & 0xF));
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_circulation_write(uint8_t value)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFB);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x23);
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value >> 4));
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value & 0xF));
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_power_write(uint8_t value)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFB);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x24);
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value >> 4));
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value & 0xF));
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_priority_write(uint8_t value)
{
    if(value == 0 || value == 0x10)
    {
        heater_rinnai_info.current_priority_location = 0x02;
        esp_timer_stop(rinnai_priority_timer);
    }
    
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFB);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x25);
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value >> 4));
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value & 0xF));
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void wifi_rinnai_command_model_upload(void)
{
    uint8_t plain_buf[48] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    plain_buf[0] = send_counter++;
    plain_buf[1] = heater_rinnai_info.type;
    plain_buf[2] = heater_rinnai_info.model;

    crypto_aes_remote_encrypt(plain_buf,3, &encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xA0, 3, send_len);

    free(encrypt_ptr);
}

void wifi_rinnai_temperature_setting_response(void)
{
    uint8_t plain_buf[48] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    plain_buf[0] = send_counter++;
    plain_buf[1] = 1;

    crypto_aes_remote_encrypt(plain_buf,2,&encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xA1, 2, send_len);

    free(encrypt_ptr);
}

void wifi_rinnai_eco_setting_response(void)
{
    uint8_t plain_buf[48] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    plain_buf[0] = send_counter++;
    plain_buf[1] = 1;

    crypto_aes_remote_encrypt(plain_buf,2,&encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xA2, 2, send_len);

    free(encrypt_ptr);
}

void wifi_rinnai_circulation_setting_response(void)
{
    uint8_t plain_buf[48] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    plain_buf[0] = send_counter++;
    plain_buf[1] = 1;

    crypto_aes_remote_encrypt(plain_buf,41,&encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xA3, 2, send_len);

    free(encrypt_ptr);
}

void wifi_rinnai_power_setting_response(void)
{
    uint8_t plain_buf[48] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    plain_buf[0] = send_counter++;
    plain_buf[1] = 1;

    crypto_aes_remote_encrypt(plain_buf,2,&encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xA4, 2, send_len);

    free(encrypt_ptr);
}

void wifi_rinnai_priority_setting_response(void)
{
    uint8_t plain_buf[48] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    plain_buf[0] = send_counter++;
    plain_buf[1] = 1;

    crypto_aes_remote_encrypt(plain_buf,2,&encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xA5, 2, send_len);

    free(encrypt_ptr);
}

void wifi_rinnai_priority_timer_callback(void* arg)
{
    heater_rinnai_info.current_priority_location = 0x02;
}

void wifi_rinnai_priority_timer_init(void)
{
    const esp_timer_create_args_t rinnai_priority_timer_args = 
    {
        .callback = &wifi_rinnai_priority_timer_callback,
        .name = "rinnai_priority_timer"
    };

    ESP_ERROR_CHECK(esp_timer_create(&rinnai_priority_timer_args, &rinnai_priority_timer));
}

void wifi_rinnai_priority_timer_refresh(void)
{
    esp_timer_stop(rinnai_priority_timer);
    esp_timer_start_once(rinnai_priority_timer, 10 * 60 * 1000 * 1000);
}

void wifi_rinnai_command_info_upload(void)
{
    uint8_t plain_buf[48] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    //copy data
    heater_rinnai_info_t heater_rinnai_info_temp = {0};
    memcpy(&heater_rinnai_info_temp,&heater_rinnai_info,sizeof(heater_rinnai_info_t));

    //data convert
    if(heater_rinnai_info_temp.error == 0xAA)
    {
        heater_rinnai_info_temp.error = 0;
    }
    for(uint8_t i = 0; i < 8; i++)
    {
        if(heater_rinnai_info_temp.error_record[i] == 0xAA)
        {
            heater_rinnai_info_temp.error_record[i] = 0;
        }
    }

    heater_rinnai_info_temp.total_flow_rate = (uint32_t)(heater_rinnai_info_temp.total_flow_rate * 0.1);
    heater_rinnai_info_temp.fan_speed = (uint8_t)(heater_rinnai_info_temp.fan_speed * 1.5);
    heater_rinnai_info_temp.power_on_time = heater_rinnai_info_temp.power_on_time * 3;
    heater_rinnai_info_temp.combustion_number_times = heater_rinnai_info_temp.combustion_number_times * 10;
    heater_rinnai_info_temp.input_water_thermistor_detection_temperature = (uint8_t)(heater_rinnai_info_temp.input_water_thermistor_detection_temperature * 0.01);
    heater_rinnai_info_temp.inlet_water_thermistor_detection_temperature = (uint8_t)(heater_rinnai_info_temp.inlet_water_thermistor_detection_temperature * 0.01);
    heater_rinnai_info_temp.gas_consumption = (uint32_t)(heater_rinnai_info_temp.gas_consumption * 100 * 4.185 / 128);

    if(heater_rinnai_info_temp.on_off_setting >= 0x31 && heater_rinnai_info_temp.on_off_setting <= 0x54)
    {
        heater_rinnai_info_temp.on_off_setting = 0;
    }
    else
    {
        heater_rinnai_info_temp.on_off_setting = 1;
    }

    //skip duplication
    if(memcmp(&heater_rinnai_info_temp,&heater_rinnai_info_last,sizeof(heater_rinnai_info_t)) == 0)
    {
        return;
    }
    else
    {
        memcpy(&heater_rinnai_info_last,&heater_rinnai_info_temp,sizeof(heater_rinnai_info_t));
    }

    plain_buf[0] = send_counter++;
    plain_buf[1] = heater_rinnai_info_temp.error;

    plain_buf[2] = heater_rinnai_info_temp.total_flow_rate & 0xFF;
    plain_buf[3] = (heater_rinnai_info_temp.total_flow_rate >> 8) & 0xFF;

    plain_buf[4] = heater_rinnai_info_temp.flow_rate_of_heat_exchanger & 0xFF;
    plain_buf[5] = (heater_rinnai_info_temp.flow_rate_of_heat_exchanger >> 8) & 0xFF;
    plain_buf[6] = heater_rinnai_info_temp.flow_rate_of_circulation_pump & 0xFF;
    plain_buf[7] = (heater_rinnai_info_temp.flow_rate_of_circulation_pump >> 8) & 0xFF;

    plain_buf[8] = heater_rinnai_info_temp.fan_speed;

    plain_buf[9] = heater_rinnai_info_temp.power_on_time & 0xFF;
    plain_buf[10] = (heater_rinnai_info_temp.power_on_time >> 8) & 0xFF;
    plain_buf[11] = (heater_rinnai_info_temp.power_on_time >> 16) & 0xFF;

    plain_buf[12] = heater_rinnai_info_temp.combustion_time & 0xFF;
    plain_buf[13] = (heater_rinnai_info_temp.combustion_time >> 8) & 0xFF;
    plain_buf[14] = (heater_rinnai_info_temp.combustion_time >> 16) & 0xFF;

    plain_buf[15] = heater_rinnai_info_temp.combustion_number_times & 0xFF;
    plain_buf[16] = (heater_rinnai_info_temp.combustion_number_times >> 8) & 0xFF;
    plain_buf[17] = (heater_rinnai_info_temp.combustion_number_times >> 16) & 0xFF;

    plain_buf[18] = heater_rinnai_info_temp.input_water_thermistor_detection_temperature;
    plain_buf[19] = heater_rinnai_info_temp.inlet_water_thermistor_detection_temperature;
    plain_buf[20] = heater_rinnai_info_temp.heat_exchanger_temperature;

    plain_buf[21] = heater_rinnai_info_temp.error_record[0];
    plain_buf[22] = heater_rinnai_info_temp.error_record[1];
    plain_buf[23] = heater_rinnai_info_temp.error_record[2];
    plain_buf[24] = heater_rinnai_info_temp.error_record[3];
    plain_buf[25] = heater_rinnai_info_temp.error_record[4];
    plain_buf[26] = heater_rinnai_info_temp.error_record[5];
    plain_buf[27] = heater_rinnai_info_temp.error_record[6];
    plain_buf[28] = heater_rinnai_info_temp.error_record[7];

    plain_buf[29] = heater_rinnai_info_temp.gas_overflow;
    plain_buf[30] = heater_rinnai_info_temp.gas_consumption & 0xFF;
    plain_buf[31] = (heater_rinnai_info_temp.gas_consumption >> 8) & 0xFF;

    plain_buf[32] = heater_rinnai_info_temp.hot_water_overflow;
    plain_buf[33] = heater_rinnai_info_temp.hot_water_consumption & 0xFF;
    plain_buf[34] = (heater_rinnai_info_temp.hot_water_consumption >> 8) & 0xFF;

    plain_buf[35] = heater_rinnai_info_temp.combustion_status;
    plain_buf[36] = heater_rinnai_info_temp.current_priority_location;
    plain_buf[37] = heater_rinnai_info_temp.current_temperature_setting;
    plain_buf[38] = heater_rinnai_info_temp.eco_status;
    plain_buf[39] = heater_rinnai_info_temp.circulation_status;
    plain_buf[40] = heater_rinnai_info_temp.on_off_setting;

    ESP_LOG_BUFFER_HEXDUMP("wifi-tx-plain_buf", plain_buf, 41, ESP_LOG_INFO);

    crypto_aes_remote_encrypt(plain_buf,41,&encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xB0, 41, send_len);

    free(encrypt_ptr);
}

void heater_rinnai_data_handle(uint8_t offset)
{
    heater_rinnai_uart_frame_t info_frame;
    unsigned char device_type = heater_data_process_buf[offset + HEATER_UART_CMD_0];
    if(device_type != 0xFB)
    {
        return;
    }

    uint16_t cmd_type = heater_data_process_buf[offset + HEATER_UART_CMD_2] << 8 | heater_data_process_buf[offset + HEATER_UART_CMD_3];
    switch(cmd_type)
    {
        case 0x2020://appliance information 
            heater_rinnai_info.type = char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START]) << 4 | char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START + 1]);
            heater_rinnai_info.model = char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START + 3]);
            ESP_LOGI(TAG,"heater_rinnai type %02X,model %02X",heater_rinnai_info.type,heater_rinnai_info.model);
            wifi_rinnai_command_model_upload();
            heater_detect_finish(HEATER_TYPE_RINNAI_HOME);
            break;
        case 0x2030://state information
            memcpy(&info_frame,&heater_data_process_buf[offset + HEATER_UART_DATA_START],sizeof(heater_rinnai_uart_frame_t));

            heater_rinnai_info.error = char_to_hex(info_frame.error[0]) << 4 | char_to_hex(info_frame.error[1]);
            heater_rinnai_info.total_flow_rate = char_to_hex(info_frame.total_flow_rate[0]) << 12 | char_to_hex(info_frame.total_flow_rate[1]) << 8 | \
                                                    char_to_hex(info_frame.total_flow_rate[2]) << 4 | char_to_hex(info_frame.total_flow_rate[3]);
            heater_rinnai_info.flow_rate_of_heat_exchanger = char_to_hex(info_frame.flow_rate_of_heat_exchanger[0]) << 8 | char_to_hex(info_frame.flow_rate_of_heat_exchanger[1]) << 4 | \
                                                            char_to_hex(info_frame.flow_rate_of_heat_exchanger[2]);
            heater_rinnai_info.flow_rate_of_circulation_pump = char_to_hex(info_frame.flow_rate_of_circulation_pump[0]) << 8 | char_to_hex(info_frame.flow_rate_of_circulation_pump[1]) << 4 |\
                                                            char_to_hex(info_frame.flow_rate_of_circulation_pump[2]);
            heater_rinnai_info.fan_speed = char_to_hex(info_frame.fan_speed[0]) << 8 | char_to_hex(info_frame.fan_speed[1]) << 4 | \
                                        char_to_hex(info_frame.fan_speed[2]);
            heater_rinnai_info.power_on_time = char_to_hex(info_frame.power_on_time[0]) << 16 | char_to_hex(info_frame.power_on_time[1]) << 12 | \
                                            char_to_hex(info_frame.power_on_time[2]) << 8 | char_to_hex(info_frame.power_on_time[3]) << 4 | \
                                                char_to_hex(info_frame.power_on_time[4]);
            heater_rinnai_info.combustion_time = char_to_hex(info_frame.combustion_time[0]) << 16 | char_to_hex(info_frame.combustion_time[1]) << 12 | \
                                                char_to_hex(info_frame.combustion_time[2]) << 8 | char_to_hex(info_frame.combustion_time[3]) << 4 | \
                                                    char_to_hex(info_frame.combustion_time[4]);
            heater_rinnai_info.combustion_number_times = char_to_hex(info_frame.combustion_number_times[0]) << 16 | char_to_hex(info_frame.combustion_number_times[1]) << 12 | \
                                                        char_to_hex(info_frame.combustion_number_times[2]) << 8 | char_to_hex(info_frame.combustion_number_times[3]) << 4 | \
                                                            char_to_hex(info_frame.combustion_number_times[4]);
            heater_rinnai_info.input_water_thermistor_detection_temperature = char_to_hex(info_frame.input_water_thermistor_detection_temperature[0]) << 16 | char_to_hex(info_frame.input_water_thermistor_detection_temperature[1]) << 12 | \
                                                                            char_to_hex(info_frame.input_water_thermistor_detection_temperature[2]) << 8 | char_to_hex(info_frame.input_water_thermistor_detection_temperature[3]) << 4 | \
                                                                                char_to_hex(info_frame.input_water_thermistor_detection_temperature[4]);
            heater_rinnai_info.inlet_water_thermistor_detection_temperature = char_to_hex(info_frame.inlet_water_thermistor_detection_temperature[0]) << 16 | char_to_hex(info_frame.inlet_water_thermistor_detection_temperature[1]) << 12 | \
                                                                            char_to_hex(info_frame.inlet_water_thermistor_detection_temperature[2]) << 8 | char_to_hex(info_frame.inlet_water_thermistor_detection_temperature[3]) << 4 | \
                                                                                char_to_hex(info_frame.inlet_water_thermistor_detection_temperature[4]);
            heater_rinnai_info.heat_exchanger_temperature = char_to_hex(info_frame.heat_exchanger_temperature[0]) << 16 | char_to_hex(info_frame.heat_exchanger_temperature[1]) << 12 | \
                                                            char_to_hex(info_frame.heat_exchanger_temperature[2]) << 8 | char_to_hex(info_frame.heat_exchanger_temperature[3]) << 4 | \
                                                                char_to_hex(info_frame.heat_exchanger_temperature[4]);
            heater_rinnai_info.error_record[0] = char_to_hex(info_frame.error_record[0]) << 4 | char_to_hex(info_frame.error_record[1]);
            heater_rinnai_info.error_record[1] = char_to_hex(info_frame.error_record[2]) << 4 | char_to_hex(info_frame.error_record[3]);            
            heater_rinnai_info.error_record[2] = char_to_hex(info_frame.error_record[4]) << 4 | char_to_hex(info_frame.error_record[5]);
            heater_rinnai_info.error_record[3] = char_to_hex(info_frame.error_record[6]) << 4 | char_to_hex(info_frame.error_record[7]);
            heater_rinnai_info.error_record[4] = char_to_hex(info_frame.error_record[8]) << 4 | char_to_hex(info_frame.error_record[9]);
            heater_rinnai_info.error_record[5] = char_to_hex(info_frame.error_record[10]) << 4 | char_to_hex(info_frame.error_record[11]);
            heater_rinnai_info.error_record[6] = char_to_hex(info_frame.error_record[12]) << 4 | char_to_hex(info_frame.error_record[13]);
            heater_rinnai_info.error_record[7] = char_to_hex(info_frame.error_record[14]) << 4 | char_to_hex(info_frame.error_record[15]);
            heater_rinnai_info.gas_consumption = char_to_hex(info_frame.gas_consumption[0]) << 16 | char_to_hex(info_frame.gas_consumption[1]) << 12 | \
                                                char_to_hex(info_frame.gas_consumption[2]) << 8 | char_to_hex(info_frame.gas_consumption[3]) << 4 | \
                                                    char_to_hex(info_frame.gas_consumption[4]);
            heater_rinnai_info.hot_water_consumption = char_to_hex(info_frame.hot_water_consumption[0]) << 16 | char_to_hex(info_frame.hot_water_consumption[1]) << 12 | \
                                                    char_to_hex(info_frame.hot_water_consumption[2]) << 8 | char_to_hex(info_frame.hot_water_consumption[3]) << 4 | \
                                                        char_to_hex(info_frame.hot_water_consumption[4]);
            heater_rinnai_info.combustion_status = char_to_hex(info_frame.combustion_status[0]) << 4 | char_to_hex(info_frame.combustion_status[1]);
            heater_rinnai_info.current_temperature_setting = char_to_hex(info_frame.current_temperature_setting[0]) << 4 | char_to_hex(info_frame.current_temperature_setting[1]);
            heater_rinnai_info.eco_status = char_to_hex(info_frame.eco_status[0]) << 4 | char_to_hex(info_frame.eco_status[1]);
            heater_rinnai_info.circulation_status = char_to_hex(info_frame.circulation_status[0]) << 4 | char_to_hex(info_frame.circulation_status[1]);
            
            if((char_to_hex(info_frame.on_off_setting[0]) << 4 | char_to_hex(info_frame.on_off_setting[1])) == 0x20)
            {
                smartconfig_reset();
            }
            else
            {
                heater_rinnai_info.on_off_setting = char_to_hex(info_frame.on_off_setting[0]) << 4 | char_to_hex(info_frame.on_off_setting[1]);
            }

            if(last_combustion_status != heater_rinnai_info.combustion_status)
            {
                last_combustion_status = heater_rinnai_info.combustion_status;
                if(heater_rinnai_info.combustion_status)
                {
                    heater_rinnai_info.current_priority_location = 0x01;
                    wifi_rinnai_priority_timer_refresh();
                }
            }
            ESP_LOGI(TAG,"heater_rinnai read state information success");
            wifi_rinnai_command_info_upload();
            break;
        case 0x3021://temperature setting
            heater_rinnai_info.current_temperature_setting = char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START]) << 4 | char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START + 1]);
            ESP_LOGI(TAG,"heater_rinnai temperature setting to %d",heater_rinnai_info.current_temperature_setting);
            wifi_rinnai_command_info_upload();
            wifi_rinnai_temperature_setting_response();
            break;
        case 0x3022://eco setting
            heater_rinnai_info.eco_status = char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START]) << 4 | char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START + 1]);
            ESP_LOGI(TAG,"heater_rinnai eco_status setting to %d",heater_rinnai_info.eco_status);
            wifi_rinnai_command_info_upload();
            wifi_rinnai_eco_setting_response();
            break;
        case 0x3023://circulation setting
            heater_rinnai_info.circulation_status = char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START]) << 4 | char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START + 1]);
            ESP_LOGI(TAG,"heater_rinnai circulation_status setting to %d",heater_rinnai_info.circulation_status);
            wifi_rinnai_command_info_upload();
            wifi_rinnai_circulation_setting_response();
            break;
        case 0x3024://power setting
            if((char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START]) << 4 | char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START + 1])) == 1)
            {
                heater_rinnai_info.on_off_setting = 0x10;
            }
            else
            {
                heater_rinnai_info.on_off_setting = 0x50;
            }
            ESP_LOGI(TAG,"heater_rinnai on_off_setting setting to %d",heater_rinnai_info.on_off_setting);
            wifi_rinnai_command_info_upload();
            wifi_rinnai_power_setting_response();
            break;
        case 0x3025://priority setting
            heater_rinnai_info.current_priority_location = char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START]) << 4 | char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START + 1]);
            ESP_LOGI(TAG,"heater_rinnai current_priority_location setting to %d",heater_rinnai_info.current_priority_location);
            wifi_rinnai_command_info_upload();
            wifi_rinnai_priority_setting_response();
            break;
        default:
            break;
    }
}