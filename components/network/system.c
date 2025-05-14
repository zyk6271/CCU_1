#include "network_typedef.h"
#include "wifi_api.h"
#include "crypto_aes.h"
#include "tcp_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "heater_interface_api.h"

static const char *TAG = "wifi_service";

volatile unsigned char wifi_data_process_buf[PROTOCOL_HEAD + WIFI_DATA_PROCESS_LMT];
volatile unsigned char wifi_uart_rx_buf[PROTOCOL_HEAD + WIFI_UART_RECV_BUF_LMT];
volatile unsigned char wifi_uart_tx_buf[PROTOCOL_HEAD + WIFIR_UART_SEND_BUF_LMT];

volatile unsigned char *wifi_queue_in = NULL;
volatile unsigned char *wifi_queue_out = NULL;

unsigned short set_wifi_uart_byte(unsigned short dest, unsigned char byte)
{
    unsigned char *obj = (unsigned char *)wifi_uart_tx_buf + DATA_START + dest;

    *obj = byte;
    dest += 1;

    return dest;
}

unsigned short set_wifi_uart_buffer(unsigned short dest, const unsigned char *src, unsigned short len)
{
    if(NULL == src) {
        return dest;
    }
    unsigned char *obj = (unsigned char *)wifi_uart_tx_buf + DATA_START + dest;

    my_memcpy(obj,src,len);

    dest += len;
    return dest;
}

unsigned char wifi_get_check_sum(unsigned char *pack, unsigned short pack_len)
{
    unsigned short i;
    unsigned char check_sum = 0;
    
    for(i = 0; i < pack_len; i ++) {
        check_sum += *pack ++;
    }
    
    return check_sum;
}

static void wifi_uart_write_data(unsigned char *in, unsigned short len)
{
    if((NULL == in) || (0 == len)) {
        return;
    }

    tcp_client_send(in,len);
}

void wifi_uart_write_frame(unsigned char control_type,unsigned long plain_len,unsigned long data_len)
{
    unsigned char check_sum = 0;

    extern uint8_t ccu_device_id[7];
    wifi_uart_tx_buf[HEAD_FIRST] = FRAME_FIRST;
    wifi_uart_tx_buf[DEVICE_TYPE] = DEVICE_TYPE_WATER_HEATER;
    wifi_uart_tx_buf[DEVICE_ADDRESS] = ccu_device_id[0];
    wifi_uart_tx_buf[DEVICE_ADDRESS + 1] = ccu_device_id[1];
    wifi_uart_tx_buf[DEVICE_ADDRESS + 2] = ccu_device_id[2];
    wifi_uart_tx_buf[DEVICE_ADDRESS + 3] = ccu_device_id[3];
    wifi_uart_tx_buf[DEVICE_ADDRESS + 4] = ccu_device_id[4];
    wifi_uart_tx_buf[DEVICE_ADDRESS + 5] = ccu_device_id[5];
    wifi_uart_tx_buf[DEVICE_ADDRESS + 6] = ccu_device_id[6];
    wifi_uart_tx_buf[CONTROL_TYPE] = control_type;
    wifi_uart_tx_buf[DATA_LENGTH] = plain_len;

    data_len += PROTOCOL_HEAD;
    check_sum = wifi_get_check_sum((unsigned char *)wifi_uart_tx_buf, data_len);
    wifi_uart_tx_buf[data_len] = check_sum;
    wifi_uart_tx_buf[data_len + 1] = FRAME_END;

    wifi_uart_write_data((unsigned char *)wifi_uart_tx_buf, data_len + 2);
}

void wifi_data_handle(unsigned short offset,uint32_t length)
{
    uint32_t decrypt_size;
    uint8_t *remote_decrypt_buffer;
    crypto_aes_remote_decrypt(&wifi_data_process_buf[offset + DATA_START],length,&remote_decrypt_buffer,&decrypt_size);
    ESP_LOGI(TAG,"data_handle is %02X",wifi_data_process_buf[offset + CONTROL_TYPE]);
    switch(wifi_data_process_buf[offset + CONTROL_TYPE])
    {
    case 0x96:
        uint8_t *local_decrypt_buffer;
        crypto_aes_local_decrypt(&wifi_data_process_buf[offset + DATA_START],length,&local_decrypt_buffer,&decrypt_size);
        crypto_remote_parse(local_decrypt_buffer);
        free(local_decrypt_buffer);
        heater_interface_status_reset();
        break;
    case 0x30:
        heater_interface_error_read();
        break;
    case 0x20:
        heater_interface_info_read();
        break;
    case 0x21:
        heater_interface_temperature_setting(remote_decrypt_buffer[1]);
        break;
    case 0x22:
        heater_interface_eco_setting(remote_decrypt_buffer[1]);
        break;
    case 0x23:
        heater_interface_circulation_setting(remote_decrypt_buffer[1]);
        break;
    case 0x24:
        heater_interface_power_setting(remote_decrypt_buffer[1]);
        break;
    case 0x25:
        heater_interface_priority_setting(remote_decrypt_buffer[1]);
        break;
    default:
        break;
    }

    free(remote_decrypt_buffer);
}

unsigned char wifi_get_queue_total_data(void)
{
  if(wifi_queue_in != wifi_queue_out)
    return 1;
  else
    return 0;
}

unsigned char wifi_queue_read_byte(void)
{
    unsigned char value = 0;

    if(wifi_queue_out != wifi_queue_in)
    {
        if(wifi_queue_out >= (unsigned char *)(wifi_uart_rx_buf + sizeof(wifi_uart_rx_buf)))
        {
            wifi_queue_out = (unsigned char *)(wifi_uart_rx_buf);
        }
        value = *wifi_queue_out ++;   
    }
  
  return value;
}

