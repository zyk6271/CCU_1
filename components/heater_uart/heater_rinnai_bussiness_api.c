#include "driver/uart.h"
#include "heater_uart.h"
#include "esp_log.h"
#include "string.h"
#include "crypto_aes.h"
#include "network_typedef.h"
#include "wifi_api.h"
#include "esp_timer.h"
#include "wifi_manager.h"
#include "heater_interface_api.h"
#include "heater_rinnai_bussiness_api.h"

static const char *TAG = "heater_rinnai_bussiness";

static heater_rinnai_bussiness_info_t heater_rinnai_bussiness_info = {0};
static heater_rinnai_bussiness_info_t heater_rinnai_bussiness_info_last = {0};

static uint8_t heater_rinnai_bussiness_temperature_convert_table[] = {60,85,55,80,50,75,45,70,45,88,45,65,45,65,45,65};

uint8_t heater_rinnai_bussiness_data_length_find(uint64_t command)
{
    uint8_t length = 0;
    switch(command)
    {
        case 0x3331343032:
        case 0x3330383032:
        case 0x3330303032:
        case 0x3238453032:
        case 0x3238383032:
        case 0x3238363032:
            length = 4;
            break;
        case 0x3237323132:
            length = 36;
            break;
        case 0x3330453031:
        case 0x3330463031:
        case 0x3331313031:
            length = 2;
            break;
        default:
            break;
    }

    return length;
}

void heater_rinnai_bussiness_poll_callback(void)
{
    static uint8_t read_id = 0;
    switch(read_id++)
    {
        case 0:
            heater_rinnai_bussiness_error_read();
            break;
        case 1:
            heater_rinnai_bussiness_total_flow_rate_read();
            break;
        case 2:
            heater_rinnai_bussiness_fan_speed_read();
            break;
        case 3:
            heater_rinnai_bussiness_poweron_time_read();
            break;
        case 4:
            heater_rinnai_bussiness_combustion_read();
            break;
        case 5:
            heater_rinnai_bussiness_combustion_times_read();
            break;
        case 6:
            heater_rinnai_bussiness_combustion_status_read();
            break;
        case 7:
            heater_rinnai_bussiness_error_record_read();
            break;
        case 8:
            heater_rinnai_bussiness_current_temp_read();
            break;
        case 9:
            heater_rinnai_bussiness_onoff_setting_read();
            break;
        default:
            read_id = 0;
            break;
    }
    wifi_rinnai_bussiness_command_info_upload();
}

void heater_rinnai_bussiness_poll_status_resset(void)
{
    memset(&heater_rinnai_bussiness_info_last,0,sizeof(heater_rinnai_bussiness_info_t));
}

void heater_rinnai_bussiness_info_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xF6);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x33);
    send_len = set_heater_uart_tx_byte(send_len,0x31);
    send_len = set_heater_uart_tx_byte(send_len,0x34);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x32);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_bussiness_error_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xF6);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x33);
    send_len = set_heater_uart_tx_byte(send_len,0x31);
    send_len = set_heater_uart_tx_byte(send_len,0x34);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x32);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_bussiness_total_flow_rate_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xF6);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x33);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x38);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x32);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_bussiness_fan_speed_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xF6);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x33);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x32);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_bussiness_poweron_time_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xF6);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x32);
    send_len = set_heater_uart_tx_byte(send_len,0x38);
    send_len = set_heater_uart_tx_byte(send_len,0x45);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x32);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_bussiness_combustion_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xF6);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x32);
    send_len = set_heater_uart_tx_byte(send_len,0x38);
    send_len = set_heater_uart_tx_byte(send_len,0x38);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x32);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_bussiness_combustion_times_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xF6);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x32);
    send_len = set_heater_uart_tx_byte(send_len,0x38);
    send_len = set_heater_uart_tx_byte(send_len,0x36);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x32);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_bussiness_combustion_status_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xF6);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x33);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x45);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x31);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_bussiness_error_record_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xF6);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x32);
    send_len = set_heater_uart_tx_byte(send_len,0x37);
    send_len = set_heater_uart_tx_byte(send_len,0x32);
    send_len = set_heater_uart_tx_byte(send_len,0x31);
    send_len = set_heater_uart_tx_byte(send_len,0x32);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_bussiness_current_temp_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xF6);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x33);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x46);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x31);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void heater_rinnai_bussiness_onoff_setting_read(void)
{
    unsigned short send_len = 0;
    send_len = set_heater_uart_tx_byte(send_len,0x02);
    send_len = set_heater_uart_tx_byte(send_len,0xF6);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x33);
    send_len = set_heater_uart_tx_byte(send_len,0x31);
    send_len = set_heater_uart_tx_byte(send_len,0x31);
    send_len = set_heater_uart_tx_byte(send_len,0x30);
    send_len = set_heater_uart_tx_byte(send_len,0x31);
    send_len = set_heater_uart_tx_byte(send_len,0x03);
    send_len = set_heater_uart_tx_crc(send_len);
    send_len = set_heater_uart_tx_byte(send_len,0x0D);
    heater_uart_tx_queue_enqueue(get_heater_uart_tx_buf(), send_len);
}

void wifi_rinnai_bussiness_command_model_upload(void)
{
    uint8_t plain_buf[48] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    plain_buf[0] = tcp_send_count_read();;
    plain_buf[1] = heater_rinnai_bussiness_info.type;
    plain_buf[2] = heater_rinnai_bussiness_info.model;

    crypto_aes_remote_encrypt(plain_buf,3, &encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xA0, 3, send_len);

    free(encrypt_ptr);
}

void wifi_rinnai_bussiness_command_info_upload(void)
{
    uint8_t plain_buf[48] = {0};
    uint8_t *encrypt_ptr;
    uint16_t send_len = 0;
    uint32_t encrypt_size = 0;

    //copy data
    heater_rinnai_bussiness_info_t heater_rinnai_bussiness_info_temp = {0};
    memcpy(&heater_rinnai_bussiness_info_temp,&heater_rinnai_bussiness_info,sizeof(heater_rinnai_bussiness_info_t));

    heater_rinnai_bussiness_info_temp.total_flow_rate = heater_rinnai_bussiness_info_temp.total_flow_rate * 0.01;
    heater_rinnai_bussiness_info_temp.power_on_time = heater_rinnai_bussiness_info_temp.power_on_time * 6;
    heater_rinnai_bussiness_info_temp.combustion_time = heater_rinnai_bussiness_info_temp.combustion_time * 100;
    heater_rinnai_bussiness_info_temp.hot_water_consumption = heater_rinnai_bussiness_info_temp.total_flow_rate * 0.25 + heater_rinnai_bussiness_info_last.hot_water_consumption; 
    heater_rinnai_bussiness_info_temp.current_temperature_setting = heater_rinnai_bussiness_temperature_convert_table[heater_rinnai_bussiness_info_temp.current_temperature_setting];
    if(heater_rinnai_bussiness_info_temp.combustion_status)
    {
        if(heater_rinnai_bussiness_info_temp.current_temperature_setting == 80)
        {
            heater_rinnai_bussiness_info_temp.gas_consumption = (((heater_rinnai_bussiness_info_temp.total_flow_rate / 5.5 * 88.2) + 37.8) * 1000 / 240 / 128) + heater_rinnai_bussiness_info_last.gas_consumption;
        }
        else
        {
            heater_rinnai_bussiness_info_temp.gas_consumption = (((heater_rinnai_bussiness_info_temp.total_flow_rate / 10 * 88.2) + 37.8) * 1000 / 240 / 128) + heater_rinnai_bussiness_info_last.gas_consumption;
        }
    }

    //skip duplication
    if(memcmp(&heater_rinnai_bussiness_info_temp,&heater_rinnai_bussiness_info_last,sizeof(heater_rinnai_bussiness_info_t)) == 0)
    {
        return;
    }
    else
    {
        memcpy(&heater_rinnai_bussiness_info_last,&heater_rinnai_bussiness_info_temp,sizeof(heater_rinnai_bussiness_info_t));
    }


    plain_buf[0] = tcp_send_count_read();;
    plain_buf[1] = heater_rinnai_bussiness_info_temp.error;

    plain_buf[2] = (uint32_t)(heater_rinnai_bussiness_info_temp.total_flow_rate * 10) & 0xFF;
    plain_buf[3] = ((uint32_t)(heater_rinnai_bussiness_info_temp.total_flow_rate * 10) >> 8) & 0xFF;

    plain_buf[4] = heater_rinnai_bussiness_info_temp.flow_rate_of_heat_exchanger & 0xFF;
    plain_buf[5] = (heater_rinnai_bussiness_info_temp.flow_rate_of_heat_exchanger >> 8) & 0xFF;
    plain_buf[6] = heater_rinnai_bussiness_info_temp.flow_rate_of_circulation_pump & 0xFF;
    plain_buf[7] = (heater_rinnai_bussiness_info_temp.flow_rate_of_circulation_pump >> 8) & 0xFF;

    plain_buf[8] = heater_rinnai_bussiness_info_temp.fan_speed;

    plain_buf[9] = heater_rinnai_bussiness_info_temp.power_on_time & 0xFF;
    plain_buf[10] = (heater_rinnai_bussiness_info_temp.power_on_time >> 8) & 0xFF;
    plain_buf[11] = (heater_rinnai_bussiness_info_temp.power_on_time >> 16) & 0xFF;

    plain_buf[12] = heater_rinnai_bussiness_info_temp.combustion_time & 0xFF;
    plain_buf[13] = (heater_rinnai_bussiness_info_temp.combustion_time >> 8) & 0xFF;
    plain_buf[14] = (heater_rinnai_bussiness_info_temp.combustion_time >> 16) & 0xFF;

    plain_buf[15] = heater_rinnai_bussiness_info_temp.combustion_number_times & 0xFF;
    plain_buf[16] = (heater_rinnai_bussiness_info_temp.combustion_number_times >> 8) & 0xFF;
    plain_buf[17] = (heater_rinnai_bussiness_info_temp.combustion_number_times >> 16) & 0xFF;

    plain_buf[18] = heater_rinnai_bussiness_info_temp.input_water_thermistor_detection_temperature;
    plain_buf[19] = heater_rinnai_bussiness_info_temp.inlet_water_thermistor_detection_temperature;
    plain_buf[20] = heater_rinnai_bussiness_info_temp.heat_exchanger_temperature;

    plain_buf[21] = heater_rinnai_bussiness_info_temp.error_record[0];
    plain_buf[22] = heater_rinnai_bussiness_info_temp.error_record[1];
    plain_buf[23] = heater_rinnai_bussiness_info_temp.error_record[2];
    plain_buf[24] = heater_rinnai_bussiness_info_temp.error_record[3];
    plain_buf[25] = heater_rinnai_bussiness_info_temp.error_record[4];
    plain_buf[26] = heater_rinnai_bussiness_info_temp.error_record[5];
    plain_buf[27] = heater_rinnai_bussiness_info_temp.error_record[6];
    plain_buf[28] = heater_rinnai_bussiness_info_temp.error_record[7];

    plain_buf[29] = heater_rinnai_bussiness_info_temp.gas_overflow;
    if(heater_rinnai_bussiness_info_temp.combustion_status)
    {
        plain_buf[30] = ((uint32_t)heater_rinnai_bussiness_info_temp.gas_consumption) & 0xFF;
        plain_buf[31] = ((uint32_t)(heater_rinnai_bussiness_info_temp.gas_consumption) >> 8) & 0xFF;
    }

    plain_buf[32] = heater_rinnai_bussiness_info_temp.hot_water_overflow;
    plain_buf[33] = ((uint32_t)heater_rinnai_bussiness_info_temp.hot_water_consumption) & 0xFF;
    plain_buf[34] = ((uint32_t)(heater_rinnai_bussiness_info_temp.hot_water_consumption) >> 8) & 0xFF;

    plain_buf[35] = heater_rinnai_bussiness_info_temp.combustion_status;
    plain_buf[36] = heater_rinnai_bussiness_info_temp.current_priority_location;
    plain_buf[37] = heater_rinnai_bussiness_info_temp.current_temperature_setting;
    plain_buf[38] = heater_rinnai_bussiness_info_temp.eco_status;
    plain_buf[39] = heater_rinnai_bussiness_info_temp.circulation_status;
    plain_buf[40] = heater_rinnai_bussiness_info_temp.on_off_setting;

    ESP_LOG_BUFFER_HEXDUMP("wifi-tx-plain_buf", plain_buf, 41, ESP_LOG_INFO);

    crypto_aes_remote_encrypt(plain_buf,41,&encrypt_ptr,&encrypt_size);

    send_len = set_wifi_uart_buffer(send_len, encrypt_ptr, encrypt_size);

    wifi_uart_write_frame(0xB0, 41, send_len);

    free(encrypt_ptr);
}

void heater_rinnai_bussiness_data_handle(uint8_t offset)
{
    unsigned char device_type = heater_data_process_buf[offset + HEATER_UART_CMD_0];
    if(device_type != 0xF6)
    {
        return;
    }

    uint64_t cmd_type = (heater_data_process_buf[offset + HEATER_UART_CMD_2] * 0x100000000) | heater_data_process_buf[offset + HEATER_UART_CMD_3] << 24 | 
                            heater_data_process_buf[offset + HEATER_UART_CMD_4] << 16 | heater_data_process_buf[offset + HEATER_UART_CMD_5] << 8 | 
                                heater_data_process_buf[offset + HEATER_UART_CMD_6];

    switch(cmd_type)
    {
        case 0x3331343032:
            heater_rinnai_bussiness_info.error = char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START]) |
                                                        char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 3] << 4);
            ESP_LOGI(TAG,"heater_rinnai_bussiness_info error is [%02X]",heater_rinnai_bussiness_info.error);
            heater_detect_finish(HEATER_TYPE_RINNAL_BUSINESS);
            break;
        case 0x3330383032:
            heater_rinnai_bussiness_info.total_flow_rate = char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START]) << 4 |
                                                                        char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 1] ) |
                                                                            char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 2]) << 12 |
                                                                                char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 3]) << 8;

            ESP_LOGI(TAG,"heater_rinnai_bussiness_info total_flow_rate is [%f]",heater_rinnai_bussiness_info.total_flow_rate);
            break;
        case 0x3330303032:
            heater_rinnai_bussiness_info.fan_speed = char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START]) << 4 |
                                                                        char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 1]) |
                                                                            char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 2]) << 12 |
                                                                                char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 3]) << 8;
            ESP_LOGI(TAG,"heater_rinnai_bussiness_info fan_speed is [%ld]",heater_rinnai_bussiness_info.fan_speed);
            break;
        case 0x3238453032:
            heater_rinnai_bussiness_info.power_on_time = char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START]) << 4 |
                                                                        char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 1]) |
                                                                            char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 2]) << 12 |
                                                                                char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 3]) << 8 ;
            ESP_LOGI(TAG,"heater_rinnai_bussiness_info power_on_time is [%ld]",heater_rinnai_bussiness_info.power_on_time);
            break;
        case 0x3238383032:
            heater_rinnai_bussiness_info.combustion_time = char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START]) << 4 |
                                                                        char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 1]) |
                                                                            char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 2]) << 12 |
                                                                                char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 3]) << 8;
            ESP_LOGI(TAG,"heater_rinnai_bussiness_info combustion_time is [%ld]",heater_rinnai_bussiness_info.combustion_time);
            break;
        case 0x3238363032:
            heater_rinnai_bussiness_info.combustion_number_times = char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START]) << 4 |
                                                                        char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 1]) |
                                                                            char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 2]) << 12 |
                                                                                char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 3]) << 8;
            ESP_LOGI(TAG,"heater_rinnai_bussiness_info combustion_number_times is [%ld]",heater_rinnai_bussiness_info.combustion_number_times);
            break;
        case 0x3237323132:
            for(uint8_t i = 0;i < 9;i++)
            {
                heater_rinnai_bussiness_info.error_record[i] = char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + i * 4]) |
                                                                    (char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 3 + i * 4])<< 4);
                if(heater_rinnai_bussiness_info.error_record[i] == 0xBB)
                {
                    heater_rinnai_bussiness_info.error_record[i] = 0;
                }
            }
            ESP_LOGI(TAG,"heater_rinnai_bussiness_info error_record list [%02X] [%02X] [%02X] [%02X] [%02X] [%02X] [%02X] [%02X] [%02X]",heater_rinnai_bussiness_info.error_record[0],
                        heater_rinnai_bussiness_info.error_record[1],heater_rinnai_bussiness_info.error_record[2],heater_rinnai_bussiness_info.error_record[3],
                            heater_rinnai_bussiness_info.error_record[4],heater_rinnai_bussiness_info.error_record[5],heater_rinnai_bussiness_info.error_record[6],
                                heater_rinnai_bussiness_info.error_record[7],heater_rinnai_bussiness_info.error_record[8]);
            break;
        case 0x3330453031:
            heater_rinnai_bussiness_info.combustion_status = char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 1]);
            ESP_LOGI(TAG,"heater_rinnai_bussiness_info combustion_status is [%d]",heater_rinnai_bussiness_info.combustion_status);
            break;
        case 0x3330463031:
            heater_rinnai_bussiness_info.current_temperature_setting = ((char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START]) << 4) |
                                                                            char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 1])) >> 1;   
            ESP_LOGI(TAG,"heater_rinnai_bussiness_info current_temperature_setting is [%02X]",heater_rinnai_bussiness_info.current_temperature_setting);
            break;
        case 0x3331313031:
            heater_rinnai_bussiness_info.on_off_setting = char_to_hex(heater_data_process_buf[offset + HEATER_UART_BUSSINESS_DATA_START + 1]);
            ESP_LOGI(TAG,"heater_rinnai_bussiness_info on_off_setting is [%d]",heater_rinnai_bussiness_info.on_off_setting);
            break;
        default:
            break;
    }
}