#ifndef __SYSTEM_H_
#define __SYSTEM_H_

#include "protocol.h"

#define SYSTEM_EXTERN   extern

//=============================================================================
//帧的字节顺序
//=============================================================================
#define         HEAD_FIRST                      0
#define         DEVICE_TYPE                     1
#define         DEVICE_ADDRESS                  2
#define         CONTROL_TYPE                    9
#define         DATA_LENGTH                     10
#define         DATA_START                      11

//=============================================================================
//设备类型
//=============================================================================
#define         DEVICE_TYPE_WATER_HEATER             0x75
#define         DEVICE_TYPE_COOKING_RANGE            0x78

//=============================================================================
#define PROTOCOL_HEAD           0x0B                                            //固定协议头长度
#define FRAME_FIRST             0x68                                            //固定数据包头
#define FRAME_END               0x16                                            //固定数据包尾
//============================================================================= 

SYSTEM_EXTERN volatile unsigned char wifi_data_process_buf[PROTOCOL_HEAD + WIFI_DATA_PROCESS_LMT];      //串口数据处理缓存
SYSTEM_EXTERN volatile unsigned char wifi_uart_rx_buf[PROTOCOL_HEAD + WIFI_UART_RECV_BUF_LMT];          //串口接收缓存
SYSTEM_EXTERN volatile unsigned char wifi_uart_tx_buf[PROTOCOL_HEAD + WIFIR_UART_SEND_BUF_LMT];         //串口发送缓存

SYSTEM_EXTERN volatile unsigned char *wifi_queue_in;
SYSTEM_EXTERN volatile unsigned char *wifi_queue_out;

/**
 * @brief  写wifi_uart字节
 * @param[in] {dest} 缓存区地址偏移
 * @param[in] {byte} 写入字节值
 * @return 写入完成后的总长度
 */
unsigned short set_wifi_uart_byte(unsigned short dest, unsigned char byte);

/**
 * @brief  写wifi_uart_buffer
 * @param[in] {dest} 缓存区地址偏移
 * @param[in] {src} 源地址（需要发送的数据）
 * @param[in] {len} 需要发送的数据长度
 * @return 写入完成后的总长度
 */
unsigned short set_wifi_uart_buffer(unsigned short dest, const unsigned char *src, unsigned short len);

/**
 * @brief  计算校验和
 * @param[in] {pack} 数据源指针
 * @param[in] {pack_len} 计算校验和长度
 * @return 校验和
 */
unsigned char wifi_get_check_sum(unsigned char *pack, unsigned short pack_len);

/**
 * @brief  向wifi串口发送一帧数据
 * @param[in] {fr_type} 帧类型
 * @param[in] {fr_ver} 帧版本
 * @param[in] {len} 数据长度
 * @return Null
 */
void wifi_uart_write_frame(unsigned char control_type,unsigned long plain_len,unsigned long data_len);

/**
 * @brief  获取制定DPID在数组中的序号
 * @param[in] {dpid} dpid
 * @return dp序号
 */
unsigned char get_dowmload_dpid_index(unsigned char dpid);

/**
 * @brief  数据帧处理
 * @param[in] {offset} 数据起始位
 * @return Null
 */
void wifi_data_handle(unsigned short offset,uint32_t length);

/**
 * @brief  判断串口接收缓存中是否有数据
 * @param  Null
 * @return 是否有数据  0:无/1:有
 */
unsigned char wifi_get_queue_total_data(void);

/**
 * @brief  读取队列1字节数据
 * @param  Null
 * @return Read the data
 */
unsigned char wifi_queue_read_byte(void);

#endif
  
  
