#include "esp_log.h"

#define MCU_API_GLOBAL

static const char *TAG = "mcu_api";

#include "network_typedef.h"

/**
 * @brief  求字符串长度
 * @param[in] {str} 字符串地址
 * @return 数据长度
 */
unsigned long my_strlen(unsigned char *str)  
{
    unsigned long len = 0;
    if(str == NULL) { 
        return 0;
    }
    
    for(len = 0; *str ++ != '\0'; ) {
        len ++;
    }
    
    return len;
}

/**
 * @brief  把src所指内存区域的前count个字节设置成字符c
 * @param[out] {src} 待设置的内存首地址
 * @param[in] {ch} 设置的字符
 * @param[in] {count} 设置的内存长度
 * @return 待设置的内存首地址
 */
void *my_memset(void *src,unsigned char ch,unsigned short count)
{
    unsigned char *tmp = (unsigned char *)src;
    
    if(src == NULL) {
        return NULL;
    }
    
    while(count --) {
        *tmp ++ = ch;
    }
    
    return src;
}

/**
 * @brief  内存拷贝
 * @param[out] {dest} 目标地址
 * @param[in] {src} 源地址
 * @param[in] {count} 拷贝数据个数
 * @return 数据处理完后的源地址
 */
void *my_memcpy(void *dest, const void *src, unsigned short count)  
{  
    unsigned char *pdest = (unsigned char *)dest;  
    const unsigned char *psrc  = (const unsigned char *)src;  
    unsigned short i;
    
    if(dest == NULL || src == NULL) { 
        return NULL;
    }
    
    if((pdest <= psrc) || (pdest > psrc + count)) {  
        for(i = 0; i < count; i ++) {  
            pdest[i] = psrc[i];  
        }  
    }else {
        for(i = count; i > 0; i --) {  
            pdest[i - 1] = psrc[i - 1];  
        }  
    }  
    
    return dest;  
}

/**
 * @brief  字符串拷贝
 * @param[in] {dest} 目标地址
 * @param[in] {src} 源地址
 * @return 数据处理完后的源地址
 */
char *my_strcpy(char *dest, const char *src)  
{
    char *p = dest;
    
    if(dest == NULL || src == NULL) { 
        return NULL;
    }
    
    while(*src!='\0') {
        *dest++ = *src++;
    }
    *dest = '\0';
    return p;
}

/**
 * @brief  字符串比较
 * @param[in] {s1} 字符串 1
 * @param[in] {s2} 字符串 2
 * @return 大小比较值
 * -         0:s1=s2
 * -         <0:s1<s2
 * -         >0:s1>s2
 */
int my_strcmp(char *s1 , char *s2)
{
    while( *s1 && *s2 && *s1 == *s2 ) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/**
 * @brief  将int类型拆分四个字节
 * @param[in] {number} 4字节原数据
 * @param[out] {value} 处理完成后4字节数据
 * @return Null
 */
void int_to_byte(unsigned long number,unsigned char value[4])
{
    value[0] = number >> 24;
    value[1] = number >> 16;
    value[2] = number >> 8;
    value[3] = number & 0xff;
}

/**
 * @brief  将4字节合并为1个32bit变量
 * @param[in] {value} 4字节数组
 * @return 合并完成后的32bit变量
 */
unsigned long byte_to_int(const unsigned char value[4])
{
    unsigned long nubmer = 0;

    nubmer = (unsigned long)value[0];
    nubmer <<= 8;
    nubmer |= (unsigned long)value[1];
    nubmer <<= 8;
    nubmer |= (unsigned long)value[2];
    nubmer <<= 8;
    nubmer |= (unsigned long)value[3];
    
    return nubmer;
}

/**
 * @brief  串口接收数据暂存处理
 * @param[in] {value} 串口收到的1字节数据
 * @return Null
 * @note   在MCU串口处理函数中调用该函数,并将接收到的数据作为参数传入
 */
void wifi_uart_receive_input(unsigned char value)
{
    if(1 == wifi_queue_out - wifi_queue_in)
    {
        ESP_LOGE(TAG,"uart_receive queue is full now");
        //数据队列满
    }
    else if((wifi_queue_in > wifi_queue_out) && ((wifi_queue_in - wifi_queue_out) >= sizeof(wifi_data_process_buf)))
    {
        ESP_LOGE(TAG,"uart_receive queue is full now");
        //数据队列满
    }
    else
    {
        //队列不满
        if(wifi_queue_in >= (unsigned char *)(wifi_uart_rx_buf + sizeof(wifi_uart_rx_buf)))
        {
            wifi_queue_in = (unsigned char *)(wifi_uart_rx_buf);
        }
        
        *wifi_queue_in ++ = value;
    }
}

/**
 * @brief  wifi串口数据处理服务
 * @param  Null
 * @return Null
 * @note   在MCU主函数while循环中调用该函数
 */
void wifi_uart_service(void)
{
    static unsigned short rx_in = 0;
    unsigned short offset = 0;
    unsigned short rx_value_len = 0;             //数据帧长度
    unsigned char check_sum;
    
    while((rx_in < sizeof(wifi_data_process_buf)) && wifi_get_queue_total_data() > 0) {
        wifi_data_process_buf[rx_in ++] = wifi_queue_read_byte();
    }
    
    if(rx_in < PROTOCOL_HEAD)
        return;

    while((rx_in - offset) >= PROTOCOL_HEAD) {
        if(wifi_data_process_buf[offset + HEAD_FIRST] != FRAME_FIRST) {
            offset ++;
            continue;
        }
        
        if(wifi_data_process_buf[offset + DEVICE_TYPE] != DEVICE_TYPE_WATER_HEATER) {
            offset ++;
            continue;
        }

        rx_value_len = wifi_data_process_buf[offset + DATA_LENGTH] + (16 - wifi_data_process_buf[offset + DATA_LENGTH] % 16);
        if(rx_value_len > sizeof(wifi_data_process_buf)) {
            offset ++;
            continue;
        }

        if(wifi_data_process_buf[offset + DATA_LENGTH + rx_value_len + 2] != FRAME_END) {
            offset ++;
            continue;
        }

        if((rx_in - offset) < rx_value_len) {
            break;
        }
        
        //数据接收完成
        check_sum = wifi_get_check_sum((unsigned char *)wifi_data_process_buf + offset,PROTOCOL_HEAD + rx_value_len);
        if( check_sum != wifi_data_process_buf[offset + DATA_LENGTH + rx_value_len + 1]) {
            //校验出错
            ESP_LOGE(TAG,"crc error (crc:0x%X  but data:0x%X)",check_sum,wifi_data_process_buf[offset + DATA_LENGTH + rx_value_len + 1]);
            offset += 2;
            continue;
        }
        wifi_data_handle(offset,rx_value_len);
        offset += rx_value_len;
    }

    rx_in -= offset;
    if(rx_in > 0) {
        my_memcpy((char *)wifi_data_process_buf,(const char *)wifi_data_process_buf + offset,rx_in);
    }
}

/**
 * @brief  协议串口初始化函数
 * @param  Null
 * @return Null
 * @note   在MCU初始化代码中调用该函数
 */
void wifi_service_queue_init(void)
{
    wifi_queue_in = (unsigned char *)wifi_uart_rx_buf;
    wifi_queue_out = (unsigned char *)wifi_uart_rx_buf;
}
