/**
 * @file system.h
 * @brief 系统层定义: 帧结构, 设备类型 (Modbus 专用)
 */

#ifndef SYSTEM_H_
#define SYSTEM_H_

#include <stdint.h>
#include "protocol.h"

/* 帧字节偏移 */
#define HEAD_FIRST      0
#define DEVICE_TYPE     1
#define DEVICE_ADDRESS  2
#define CONTROL_TYPE    9
#define DATA_LENGTH     10
#define DATA_START      11

/* 设备类型 */
#define DEVICE_TYPE_MODBUS  0x75

/* 帧常量 */
#define PROTOCOL_HEAD   0x0B
#define FRAME_FIRST     0x68
#define FRAME_END       0x16

/* 全局缓冲区 (volatile: 可能在中断/多任务中访问) */
extern volatile unsigned char wifi_data_process_buf[PROTOCOL_HEAD + WIFI_DATA_PROCESS_LMT];
extern volatile unsigned char wifi_uart_rx_buf[PROTOCOL_HEAD + WIFI_UART_RECV_BUF_LMT];
extern volatile unsigned char wifi_uart_tx_buf[PROTOCOL_HEAD + WIFIR_UART_SEND_BUF_LMT];

extern volatile unsigned char *wifi_queue_in;
extern volatile unsigned char *wifi_queue_out;

/* 帧构建 */
unsigned short set_wifi_uart_byte(unsigned short dest, unsigned char byte);
unsigned short set_wifi_uart_buffer(unsigned short dest, const unsigned char *src, unsigned short len);
unsigned char wifi_get_check_sum(unsigned char *pack, unsigned short pack_len);
void wifi_uart_write_frame(unsigned char control_type, unsigned long plain_len, unsigned long data_len);

/* 接收处理 */
void wifi_data_handle(unsigned short offset, uint32_t length);

/* 队列操作 */
unsigned char wifi_get_queue_total_data(void);
unsigned char wifi_queue_read_byte(void);

#endif /* SYSTEM_H_ */