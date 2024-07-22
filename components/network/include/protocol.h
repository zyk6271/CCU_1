#ifndef __PROTOCOL_H_
#define __PROTOCOL_H_
#endif

#define PACKAGE_SIZE                     0x02      //1024byte
/******************************************************************************
                         3:定义收发缓存:
                    用户根据实际情况定义收发缓存的大小
******************************************************************************/
#define WIFI_UART_RECV_BUF_LMT               256              //串口数据接收缓存区大小,如MCU的RAM不够,可缩小
#define WIFI_DATA_PROCESS_LMT                256             // 单包256byte
#define WIFIR_UART_SEND_BUF_LMT              256              //根据用户DP数据大小量定，用户可根据实际情况修改

/**
 * @brief  dp下发处理函数
 * @param[in] {dpid} dpid 序号
 * @param[in] {value} dp数据缓冲区地址
 * @param[in] {length} dp数据长度
 * @return dp处理结果
 * -           0(ERROR): 失败
 * -           1(SUCCESS): 成功
 * @note   该函数用户不能修改
 */
unsigned char dp_download_handle(unsigned char dpid,const unsigned char value[], unsigned short length,unsigned char* sub_id,unsigned char sub_id_len);

