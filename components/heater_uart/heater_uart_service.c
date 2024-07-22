#include <string.h>
#include <stdlib.h>
#include "heater_uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "wifi_api.h"
#include "heater_rinnai_api.h"
#include "heater_noritz_api.h"

struct heater_uart_send_msg
{
    uint8_t *data_ptr;    /* 数据块首地址 */
    uint32_t data_size;   /* 数据块大小   */
};

unsigned char heater_data_process_buf[HEATER_UART_FRAME_MIN_SIZE + HEATER_UART_RECV_BUF_LMT];
unsigned char heater_uart_rx_buf[HEATER_UART_FRAME_MIN_SIZE + HEATER_UART_RECV_BUF_LMT];
unsigned char heater_uart_tx_buf[HEATER_UART_FRAME_MIN_SIZE + HEATER_UART_RECV_BUF_LMT];

unsigned char *heater_queue_in = NULL;
unsigned char *heater_queue_out = NULL;

uint8_t heater_tx_queue_buffer[ 10 * 256 ];
static QueueHandle_t heater_tx_queue;
static StaticQueue_t heater_tx_queue_static;

void heater_uart_receive_input(unsigned char value)
{
    if(1 == heater_queue_out - heater_queue_in)
    {
        printf("heater_uart_receive queue is full now\r\n");
    }
    else if((heater_queue_in > heater_queue_out) && ((heater_queue_in - heater_queue_out) >= sizeof(heater_data_process_buf)))
    {
        printf("heater_uart_receive queue is full now\r\n");
    }
    else
    {
        if(heater_queue_in >= (unsigned char *)(heater_uart_rx_buf + sizeof(heater_uart_rx_buf)))
        {
            heater_queue_in = (unsigned char *)(heater_uart_rx_buf);
        }
        *heater_queue_in ++ = value;
    }
}

void heater_recv_buffer(uint8_t *data,uint32_t length)
{
    while(length--)
    {
        heater_uart_receive_input(*data++);
    }
}

unsigned short set_heater_uart_tx_byte(unsigned short dest, unsigned char byte)
{
    unsigned char *obj = (unsigned char *)heater_uart_tx_buf + dest;

    *obj = byte;
    dest += 1;

    return dest;
}

unsigned short set_heater_uart_buffer(unsigned short dest, const unsigned char *src, unsigned short len)
{
    unsigned char *obj = (unsigned char *)heater_uart_tx_buf + dest;

    memcpy(obj,src,len);
    dest += len;

    return dest;
}

unsigned char *get_heater_uart_tx_buf(void)
{
    return heater_uart_tx_buf;
}

unsigned char heater_queue_read_byte(void)
{
    unsigned char value = 0;

    if(heater_queue_out != heater_queue_in)
    {
        if(heater_queue_out >= (unsigned char *)(heater_uart_rx_buf + sizeof(heater_uart_rx_buf)))
        {
            heater_queue_out = (unsigned char *)(heater_uart_rx_buf);
        }
        value = *heater_queue_out ++;
    }

    return value;
}

unsigned char heater_get_queue_total_data(void)
{
    if(heater_queue_in != heater_queue_out)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

unsigned char char_to_hex(unsigned char value)
{	
    if (value & 0x80) 
    {
        return value;
    }
	if( (value >= '0')&&(value <= '9') )
	{
		return value - '0'; //0-9
	}
	else if( (value >= 'a')&&(value <= 'f') )
	{
		return value - 'a' + 10; //10-15
	}
	else if( (value >= 'A')&&(value <= 'F') )
	{
		return value - 'A' + 10; //10-15
	}
    
	return value;
}

unsigned char hex_to_char(unsigned char hex)
{
	unsigned char str;
    if(hex <= 9)
    {
        str = hex + 0x30;
    }
    else if((hex >= 0x0A) && (hex <= 0x0F)) 
    {
        str = 0x41 + (hex - 0x0A);
    }
    else 
    {
        str = 0xff;
    }

    return str;
}

uint16_t heater_get_check_sum(unsigned char *pack, unsigned short pack_len)
{
    unsigned short i;
    uint8_t check_sum = 0;
    uint16_t output_sum = 0;

    for(i = 0; i < pack_len; i ++) 
    {
        check_sum += *pack ++;
    }
    output_sum = (hex_to_char(((check_sum >> 4) & 0x0F)) << 8) | (hex_to_char((check_sum & 0x0F)));

    return output_sum;
}

unsigned short set_heater_uart_tx_crc(unsigned short length)
{
    unsigned char *obj = (unsigned char *)heater_uart_tx_buf + length;

    uint16_t crc = heater_get_check_sum(heater_uart_tx_buf + 1,length - 1);
    *obj = crc >> 8;
    obj ++;
    *obj = crc & 0xFF;
    length += 2;

    return length;
}

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

uint8_t heater_noritz_data_length_find(uint16_t command)
{
    uint8_t length = 0;
    switch(command)
    {
        case 0x2020:
            length = 4;
            break;
        case 0x2030:
        case 0x3021:
        case 0x3022:
        case 0x3023:
        case 0x3024:
        case 0x3025:
            length = 75;
            break;
        default:
            break;
    }

    return length;
}
uint8_t heater_uart_data_length_find(uint8_t device_type,uint16_t command)
{
    if(device_type == 0xFB)
    {
        return heater_rinnai_data_length_find(command);
    }
    else
    {
        return heater_noritz_data_length_find(command);
    }
}

void heater_uart_service(void)
{
    static unsigned short rx_in = 0;
    unsigned short offset = 0;
    unsigned short rx_value_len = 0;            
    uint16_t rx_command = 0;             
    uint16_t calc_sum;
    uint16_t src_sum;

    while((rx_in < sizeof(heater_data_process_buf)) && heater_get_queue_total_data() > 0) {
        heater_data_process_buf[rx_in ++] = heater_queue_read_byte();
    }
    
    if(rx_in < HEATER_UART_FRAME_MIN_SIZE)
        return;

    while((rx_in - offset) >= HEATER_UART_FRAME_MIN_SIZE) {
        if(heater_data_process_buf[offset + HEATER_UART_HEAD_FIRST] != HEATER_UART_FRAME_HEAD_FIRST) {
            offset ++;
            continue;
        }

        if(heater_data_process_buf[offset + HEATER_UART_CMD_0] != HEATER_UART_FRAME_NORITZ_CMD_0 && 
                heater_data_process_buf[offset + HEATER_UART_CMD_0] != HEATER_UART_FRAME_RINNAI_CMD_0)
         {
            offset ++;
            continue;
        }

        if(heater_data_process_buf[offset + HEATER_UART_CMD_1] != HEATER_UART_FRAME_CMD_1) {
            offset ++;
            continue;
        }

        rx_command = heater_data_process_buf[offset + HEATER_UART_CMD_2] << 8 | heater_data_process_buf[offset + HEATER_UART_CMD_3];
        rx_value_len = heater_uart_data_length_find(heater_data_process_buf[offset + HEATER_UART_CMD_0],rx_command);

        if(heater_data_process_buf[offset + rx_value_len + 5] != HEATER_UART_FRAME_END_EXT) {
            offset ++;
            continue;
        }

        if(heater_data_process_buf[offset + rx_value_len + 8] != HEATER_UART_FRAME_END_CR) {
            offset ++;
            continue;
        }

        calc_sum = heater_get_check_sum((unsigned char *)heater_data_process_buf + offset + 1,4 + rx_value_len + 1);
        src_sum = (heater_data_process_buf[offset + rx_value_len + 6] << 8 | heater_data_process_buf[offset + rx_value_len + 7]);
        if( calc_sum != src_sum) {
            //校验出错
            printf("crc error (calc:0x%X  but data:0x%X)\r\n",calc_sum,src_sum);
            offset ++;
            continue;
        }

        if((rx_in - offset) < rx_value_len) {
            break;
        }

        printf("recv frame success,rx_command is %04X,rx_value_len is %d,checksum %04X\r\n",rx_command,rx_value_len,calc_sum);
        heater_rinnai_data_handle(offset);
        heater_noritz_data_handle(offset);
        offset += rx_value_len;
    }

    rx_in -= offset;
    if(rx_in > 0) {
        memcpy((char *)heater_data_process_buf,(const char *)heater_data_process_buf + offset,rx_in);
    }
}


void heater_uart_service_queue_init(void)
{
    heater_queue_in = (unsigned char *)heater_uart_rx_buf;
    heater_queue_out = (unsigned char *)heater_uart_rx_buf;
}

void heater_uart_service_callback(void *parameter)
{
    while(1)
    {
        heater_uart_service();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void heater_uart_tx_queue_enqueue(uint8_t *data,uint32_t length)
{
    struct heater_uart_send_msg msg_ptr;

    msg_ptr.data_ptr = data;  /* 指向相应的数据块地址 */
    msg_ptr.data_size = length; /* 数据块的长度 */

    xQueueSend(heater_tx_queue, &msg_ptr, 0);
}

void heater_uart_tx_queue_handle_callback(void *parameter)
{
    struct heater_uart_send_msg msg_ptr; /* 用于放置消息的局部变量 */
    while (1) 
    {
        xQueueReceive(heater_tx_queue, (void*)&msg_ptr, portMAX_DELAY);
        uart_write_bytes(UART_NUM_0, msg_ptr.data_ptr, msg_ptr.data_size);
        // ESP_LOG_BUFFER_HEXDUMP("heater_uart_tx_queue", msg_ptr.data_ptr, msg_ptr.data_size, ESP_LOG_INFO);
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
}

void heater_uart_service_init(void)
{
    heater_tx_queue = xQueueCreateStatic(10, sizeof(struct heater_uart_send_msg), heater_tx_queue_buffer, &heater_tx_queue_static);
    heater_uart_service_queue_init();
    xTaskCreate(heater_uart_tx_queue_handle_callback, "heater_uart_tx_queue_handle", 4096, NULL, 5, NULL);
    xTaskCreate(heater_uart_service_callback, "heater_uart_service", 4096, NULL, 6, NULL);
}