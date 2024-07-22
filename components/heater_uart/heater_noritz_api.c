#include "driver/uart.h"
#include "heater_uart.h"
#include "heater_noritz_api.h"
#include "esp_log.h"
#include "string.h"
#include "crypto_aes.h"
#include "network_typedef.h"
#include "wifi_api.h"

extern uint32_t send_counter;

noritz_heater_info_t noritz_heater;
noritz_heater_info_t noritz_heater_last;

void heater_noritz_poll_status_resset(void)
{
    memset(&noritz_heater_last,0,sizeof(noritz_heater_info_t));
}

void heater_noritz_info_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFC);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x20);
    send_len = set_heater_uart_tx_byte(send_len,0x20);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_noritz_error_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFC);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x20);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_noritz_temperature_write(uint8_t value)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFC);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x21);
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value / 10));
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value % 10));
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_noritz_eco_write(uint8_t value)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFC);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x22);
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value / 10));
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value % 10));
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_noritz_circulation_write(uint8_t value)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFC);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x23);
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value / 10));
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value % 10));
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_noritz_power_write(uint8_t value)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFC);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x24);
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value / 10));
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value % 10));
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_noritz_priority_write(uint8_t value)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xFC);
    send_len = set_heater_uart_tx_byte(send_len,0x40);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x25);
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value / 10));
    send_len = set_heater_uart_tx_byte(send_len,hex_to_char(value % 10));
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void wifi_noritz_command_model_upload(void)
{
    uint8_t plain_buf[48] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    plain_buf[0] = send_counter++;
    plain_buf[1] = noritz_heater.type;
    plain_buf[2] = noritz_heater.model;

    crypto_aes_remote_encrypt(plain_buf,3, &encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xA0, 3, send_len);

    free(encrypt_ptr);
}

void wifi_noritz_temperature_setting_response(void)
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

void wifi_noritz_eco_setting_response(void)
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

void wifi_noritz_circulation_setting_response(void)
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

void wifi_noritz_power_setting_response(void)
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

void wifi_noritz_priority_setting_response(void)
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


void wifi_noritz_command_info_upload(void)
{
    uint8_t plain_buf[48] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    //copy data
    noritz_heater_info_t noritz_heater_temp;
    memcpy(&noritz_heater_temp,&noritz_heater,sizeof(noritz_heater_info_t));

    //data convert
    noritz_heater_temp.fan_speed = (uint8_t)(noritz_heater_temp.fan_speed * 2.5);

    //skip duplication
    if(memcmp(&noritz_heater_temp,&noritz_heater_last,sizeof(noritz_heater_info_t)) == 0)
    {
        printf("wifi_noritz_command_info_upload skip duplication\r\n");
        return;
    }
    else
    {
        memcpy(&noritz_heater_last,&noritz_heater_temp,sizeof(noritz_heater_info_t));
    }

    plain_buf[0] = send_counter++;
    plain_buf[1] = noritz_heater_temp.error;
    plain_buf[2] = noritz_heater_temp.total_flow_rate & 0xFF;
    plain_buf[3] = (noritz_heater_temp.total_flow_rate >> 8) & 0xFF;


    plain_buf[4] = noritz_heater_temp.flow_rate_of_heat_exchanger & 0xFF;
    plain_buf[5] = (noritz_heater_temp.flow_rate_of_heat_exchanger >> 8) & 0xFF;
    plain_buf[6] = noritz_heater_temp.flow_rate_of_circulation_pump & 0xFF;
    plain_buf[7] = (noritz_heater_temp.flow_rate_of_circulation_pump >> 8) & 0xFF;

    plain_buf[8] = noritz_heater_temp.fan_speed;

    plain_buf[9] = noritz_heater_temp.power_on_time & 0xFF;
    plain_buf[10] = (noritz_heater_temp.power_on_time >> 8) & 0xFF;
    plain_buf[11] = (noritz_heater_temp.power_on_time >> 16) & 0xFF;

    plain_buf[12] = noritz_heater_temp.combustion_time & 0xFF;
    plain_buf[13] = (noritz_heater_temp.combustion_time >> 8) & 0xFF;
    plain_buf[14] = (noritz_heater_temp.combustion_time >> 16) & 0xFF;

    plain_buf[15] = noritz_heater_temp.combustion_number_times & 0xFF;
    plain_buf[16] = (noritz_heater_temp.combustion_number_times >> 8) & 0xFF;
    plain_buf[17] = (noritz_heater_temp.combustion_number_times >> 16) & 0xFF;

    plain_buf[18] = noritz_heater_temp.input_water_thermistor_detection_temperature;
    plain_buf[19] = noritz_heater_temp.inlet_water_thermistor_detection_temperature;
    plain_buf[20] = noritz_heater_temp.heat_exchanger_temperature;

    plain_buf[21] = noritz_heater_temp.error_record[0];
    plain_buf[22] = noritz_heater_temp.error_record[1];
    plain_buf[23] = noritz_heater_temp.error_record[2];
    plain_buf[24] = noritz_heater_temp.error_record[3];
    plain_buf[25] = noritz_heater_temp.error_record[4];
    plain_buf[26] = noritz_heater_temp.error_record[5];
    plain_buf[27] = noritz_heater_temp.error_record[6];
    plain_buf[28] = noritz_heater_temp.error_record[7];

    plain_buf[29] = noritz_heater_temp.gas_overflow;
    plain_buf[30] = noritz_heater_temp.gas_consumption & 0xFF;
    plain_buf[31] = (noritz_heater_temp.gas_consumption >> 8) & 0xFF;

    plain_buf[32] = noritz_heater_temp.hot_water_overflow;
    plain_buf[33] = noritz_heater_temp.hot_water_consumption & 0xFF;
    plain_buf[34] = (noritz_heater_temp.hot_water_consumption >> 8) & 0xFF;

    plain_buf[35] = noritz_heater_temp.combustion_status;
    plain_buf[36] = noritz_heater_temp.current_priority_location;
    plain_buf[37] = noritz_heater_temp.current_temperature_setting;
    plain_buf[38] = noritz_heater_temp.eco_status;
    plain_buf[39] = noritz_heater_temp.circulation_status;
    plain_buf[40] = noritz_heater_temp.on_off_setting;

    ESP_LOG_BUFFER_HEXDUMP("tx-plain_buf", plain_buf, 41, ESP_LOG_INFO);

    crypto_aes_remote_encrypt(plain_buf,41,&encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xB0, 41, send_len);

    free(encrypt_ptr);
}

void heater_noritz_data_handle(uint8_t offset)
{
    noritz_state_info_frame_t info_frame;

    unsigned char device_type = heater_data_process_buf[offset + HEATER_UART_CMD_0];
    if(device_type != 0xFC)
    {
        return;
    }

    uint16_t cmd_type = heater_data_process_buf[offset + HEATER_UART_CMD_2] << 8 | heater_data_process_buf[offset + HEATER_UART_CMD_3];
    switch(cmd_type)
    {
        case 0x2020://appliance information 
            noritz_heater.type = char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START]) * 10 + char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START + 1]);
            noritz_heater.model = char_to_hex(heater_data_process_buf[offset + HEATER_UART_DATA_START + 3]);
            printf("heater_noritz type %02X,model %02X\r\n",noritz_heater.type,noritz_heater.model);
            wifi_noritz_command_model_upload();
            heater_detect_finish(1);
            heater_poll_timer_start();
			heater_heart_timer_start();
            break;
        case 0x3021://temperature setting
        case 0x3022://eco setting
        case 0x3023://circulation setting
        case 0x3024://power setting
        case 0x3025://priority setting
        case 0x2030://state information
            memcpy(&info_frame,&heater_data_process_buf[offset + HEATER_UART_DATA_START],sizeof(noritz_state_info_frame_t));

            noritz_heater.error = char_to_hex(info_frame.error[0]) * 10 + char_to_hex(info_frame.error[1]);
            noritz_heater.total_flow_rate = char_to_hex(info_frame.total_flow_rate[0]) * 100 + char_to_hex(info_frame.total_flow_rate[1]) * 10 + \
                                                    char_to_hex(info_frame.total_flow_rate[2]);
            noritz_heater.flow_rate_of_heat_exchanger = char_to_hex(info_frame.flow_rate_of_heat_exchanger[0]) * 100 + char_to_hex(info_frame.flow_rate_of_heat_exchanger[1]) * 10 + \
                                                            char_to_hex(info_frame.flow_rate_of_heat_exchanger[2]);
            noritz_heater.flow_rate_of_circulation_pump = char_to_hex(info_frame.flow_rate_of_circulation_pump[0]) * 100 + char_to_hex(info_frame.flow_rate_of_circulation_pump[1]) * 10 +\
                                                            char_to_hex(info_frame.flow_rate_of_circulation_pump[2]);
            noritz_heater.fan_speed = char_to_hex(info_frame.fan_speed[0]) * 100 + char_to_hex(info_frame.fan_speed[1]) * 10 + \
                                        char_to_hex(info_frame.fan_speed[2]);
            noritz_heater.power_on_time = char_to_hex(info_frame.power_on_time[0]) * 10000 + char_to_hex(info_frame.power_on_time[1]) * 1000 + \
                                            char_to_hex(info_frame.power_on_time[2]) * 100 + char_to_hex(info_frame.power_on_time[3]) * 10 + \
                                                char_to_hex(info_frame.power_on_time[4]);
            noritz_heater.combustion_time = char_to_hex(info_frame.combustion_time[0]) * 10000 + char_to_hex(info_frame.combustion_time[1]) * 1000 + \
                                                char_to_hex(info_frame.combustion_time[2]) * 100 + char_to_hex(info_frame.combustion_time[3]) * 10 + \
                                                    char_to_hex(info_frame.combustion_time[4]);
            noritz_heater.combustion_number_times = char_to_hex(info_frame.combustion_number_times[0]) * 10000 + char_to_hex(info_frame.combustion_number_times[1]) * 1000 + \
                                                        char_to_hex(info_frame.combustion_number_times[2]) * 100 + char_to_hex(info_frame.combustion_number_times[3]) * 10 + \
                                                            char_to_hex(info_frame.combustion_number_times[4]);
            noritz_heater.input_water_thermistor_detection_temperature = char_to_hex(info_frame.input_water_thermistor_detection_temperature[0]) * 10 + char_to_hex(info_frame.input_water_thermistor_detection_temperature[1]);
            noritz_heater.inlet_water_thermistor_detection_temperature = char_to_hex(info_frame.inlet_water_thermistor_detection_temperature[0]) * 10 + char_to_hex(info_frame.inlet_water_thermistor_detection_temperature[1]);
            noritz_heater.heat_exchanger_temperature = char_to_hex(info_frame.heat_exchanger_temperature[0]) * 10 + char_to_hex(info_frame.heat_exchanger_temperature[1]);
            noritz_heater.error_record[0] = char_to_hex(info_frame.error_record[0]) * 10 + char_to_hex(info_frame.error_record[1]);
            noritz_heater.error_record[1] = char_to_hex(info_frame.error_record[2]) * 10 + char_to_hex(info_frame.error_record[3]);            
            noritz_heater.error_record[2] = char_to_hex(info_frame.error_record[4]) * 10 + char_to_hex(info_frame.error_record[5]);
            noritz_heater.error_record[3] = char_to_hex(info_frame.error_record[6]) * 10 + char_to_hex(info_frame.error_record[7]);
            noritz_heater.error_record[4] = char_to_hex(info_frame.error_record[8]) * 10 + char_to_hex(info_frame.error_record[9]);
            noritz_heater.error_record[5] = char_to_hex(info_frame.error_record[10]) * 10 + char_to_hex(info_frame.error_record[11]);
            noritz_heater.error_record[6] = char_to_hex(info_frame.error_record[12]) * 10 + char_to_hex(info_frame.error_record[13]);
            noritz_heater.error_record[7] = char_to_hex(info_frame.error_record[14]) * 10 + char_to_hex(info_frame.error_record[15]);
            noritz_heater.gas_overflow = char_to_hex(info_frame.gas_overflow);
            noritz_heater.gas_consumption = char_to_hex(info_frame.gas_consumption[0]) * 10000 + char_to_hex(info_frame.gas_consumption[1]) * 1000 + \
                                                char_to_hex(info_frame.gas_consumption[2]) * 100 + char_to_hex(info_frame.gas_consumption[3]) * 10 + \
                                                    char_to_hex(info_frame.gas_consumption[4]);
            noritz_heater.hot_water_overflow = char_to_hex(info_frame.hot_water_overflow);
            noritz_heater.hot_water_consumption = char_to_hex(info_frame.hot_water_consumption[0]) * 10000 + char_to_hex(info_frame.hot_water_consumption[1]) * 1000 + \
                                                    char_to_hex(info_frame.hot_water_consumption[2]) * 100 + char_to_hex(info_frame.hot_water_consumption[3]) * 10 + \
                                                        char_to_hex(info_frame.hot_water_consumption[4]);
            noritz_heater.combustion_status = char_to_hex(info_frame.combustion_status[0]) * 10 + char_to_hex(info_frame.combustion_status[1]);
            noritz_heater.current_priority_location = char_to_hex(info_frame.current_priority_location[0]) * 10 + char_to_hex(info_frame.current_priority_location[1]);
            noritz_heater.current_temperature_setting = char_to_hex(info_frame.current_temperature_setting[0]) * 10 + char_to_hex(info_frame.current_temperature_setting[1]);
            noritz_heater.eco_status = char_to_hex(info_frame.eco_status[0]) * 10 + char_to_hex(info_frame.eco_status[1]);
            noritz_heater.circulation_status = char_to_hex(info_frame.circulation_status[0]) * 10 + char_to_hex(info_frame.circulation_status[1]);
            noritz_heater.on_off_setting = char_to_hex(info_frame.on_off_setting[0]) * 10 + char_to_hex(info_frame.on_off_setting[1]);
            wifi_noritz_command_info_upload();
            printf("heater_noritz temperature %d\r\n",noritz_heater.current_temperature_setting);
            printf("heater_noritz eco_status %d\r\n",noritz_heater.eco_status);
            printf("heater_noritz on_off_setting %d\r\n",noritz_heater.on_off_setting);
            break;
        default:
            break;
    }
}

