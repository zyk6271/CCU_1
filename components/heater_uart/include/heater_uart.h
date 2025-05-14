//=============================================================================
//帧的字节顺序
//=============================================================================
#define         HEATER_UART_HEAD_FIRST                      0
#define         HEATER_UART_CMD_0                           1
#define         HEATER_UART_CMD_1                           2
#define         HEATER_UART_CMD_2                           3
#define         HEATER_UART_CMD_3                           4
#define         HEATER_UART_CMD_4                           5
#define         HEATER_UART_CMD_5                           6
#define         HEATER_UART_CMD_6                           7
#define         HEATER_UART_DATA_START                      5
#define         HEATER_UART_BUSSINESS_DATA_START            8

//=============================================================================
#define HEATER_UART_FRAME_HEAD_FIRST                    0x02                                            //固定唤醒
#define HEATER_UART_FRAME_NORITZ_CMD_0                  0xFC                                            //固定数据包头
#define HEATER_UART_FRAME_RINNAI_CMD_0                  0xFB                                            //固定数据包头
#define HEATER_UART_FRAME_RINNAI_BUSSINESS_CMD_0        0xF6                                            //固定数据包头
#define HEATER_UART_FRAME_CMD_1                         0x40                                            //固定数据包头
#define HEATER_UART_FRAME_BUSSINESS_CMD_1               0x30                                            //固定数据包头
#define HEATER_UART_FRAME_END_EXT                       0x03                                            //固定数据包尾
#define HEATER_UART_FRAME_END_CR                        0x0D                                            //固定数据包尾

//============================================================================= 
#define HEATER_UART_RECV_BUF_LMT               2048              //串口数据接收缓存区大小,如MCU的RAM不够,可缩小
#define HEATER_UART_PROCESS_LMT                2048             // 单包256byte
#define HEATER_UART_SEND_BUF_LMT               2048              //根据用户DP数据大小量定，用户可根据实际情况修改

#define HEATER_UART_FRAME_MIN_SIZE             10              

extern unsigned char heater_data_process_buf[HEATER_UART_FRAME_MIN_SIZE + HEATER_UART_RECV_BUF_LMT];
extern unsigned char heater_uart_rx_buf[HEATER_UART_FRAME_MIN_SIZE + HEATER_UART_RECV_BUF_LMT];
extern unsigned char heater_uart_tx_buf[HEATER_UART_FRAME_MIN_SIZE + HEATER_UART_RECV_BUF_LMT];

extern unsigned char *heater_queue_in;
extern unsigned char *heater_queue_out;

void heter_uart_init(void);
void heater_uart_service_init(void);
void heater_recv_buffer(uint8_t *data,uint32_t length);
void heater_uart_transmit_output(unsigned char value);
unsigned char hex_to_char(unsigned char hex);
unsigned char char_to_hex(unsigned char value);
unsigned char *get_heater_uart_tx_buf(void);
unsigned short set_heater_uart_tx_byte(unsigned short dest, unsigned char byte);
unsigned short set_heater_uart_buffer(unsigned short dest, const unsigned char *src, unsigned short len);
unsigned short set_heater_uart_tx_crc(unsigned short length);
void heater_uart_tx_queue_enqueue(uint8_t *data,uint32_t length);