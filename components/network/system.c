/**
 * @file system.c
 * @brief 系统层: TG 帧编码/解码, TCP 数据处理 (Modbus 专用)
 */

#include <string.h>
#include <stdlib.h>

#include "network_typedef.h"
#include "wifi_api.h"
#include "crypto_aes.h"
#include "ccu_modbus_api.h"
#include "tcp_service.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "system";

volatile unsigned char wifi_data_process_buf[PROTOCOL_HEAD + WIFI_DATA_PROCESS_LMT];
volatile unsigned char wifi_uart_rx_buf[PROTOCOL_HEAD + WIFI_UART_RECV_BUF_LMT];
volatile unsigned char wifi_uart_tx_buf[PROTOCOL_HEAD + WIFIR_UART_SEND_BUF_LMT];

volatile unsigned char *wifi_queue_in = NULL;
volatile unsigned char *wifi_queue_out = NULL;

unsigned short set_wifi_uart_byte(unsigned short dest, unsigned char byte)
{
    unsigned char *obj = (unsigned char *)wifi_uart_tx_buf + DATA_START + dest;
    *obj = byte;
    return dest + 1;
}

unsigned short set_wifi_uart_buffer(unsigned short dest, const unsigned char *src, unsigned short len)
{
    if (NULL == src)
    {
        return dest;
    }

    unsigned char *obj = (unsigned char *)wifi_uart_tx_buf + DATA_START + dest;
    memcpy(obj, src, len);
    return dest + len;
}

unsigned char wifi_get_check_sum(unsigned char *pack, unsigned short pack_len)
{
    unsigned short i;
    unsigned char check_sum = 0;

    for (i = 0; i < pack_len; i++)
    {
        check_sum += *pack++;
    }
    return check_sum;
}

/**
 * @brief 通过 TCP socket 发送组帧完毕的数据
 */
static void wifi_uart_write_data(unsigned char *in, unsigned short len)
{
    if (NULL == in || 0 == len)
    {
        return;
    }
    tcp_service_send(in, len);
}

void wifi_uart_write_frame(unsigned char control_type, unsigned long plain_len,
                           unsigned long data_len)
{
    unsigned char check_sum = 0;

    extern uint8_t ccu_device_id[7];
    wifi_uart_tx_buf[HEAD_FIRST] = FRAME_FIRST;
    wifi_uart_tx_buf[DEVICE_TYPE] = DEVICE_TYPE_MODBUS;
    wifi_uart_tx_buf[DEVICE_ADDRESS]     = ccu_device_id[0];
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

/**
 * @brief TCP 接收数据处理
 *
 * TG 模式 (encrypt_enabled=1):
 *   - 所有数据都用 Remote Key 解密
 *   - 0x96: 密钥交换响应, 用 Local Key 解密获取 Remote Key/IV
 *
 * 自定义 TCP 模式 (encrypt_enabled=0):
 *   - 数据为明文, 不解密
 *   - 不支持 0x96 密钥交换
 */
void wifi_data_handle(unsigned short offset, uint32_t length)
{
    uint8_t ctrl_type = wifi_data_process_buf[offset + CONTROL_TYPE];
    const volatile uint8_t *data_ptr = &wifi_data_process_buf[offset + DATA_START];

    ESP_LOGI(TAG, "data_handle ctrl=0x%02X len=%lu", ctrl_type, (unsigned long)length);

    if (tcp_service_encrypt_enabled_get())
    {
        /* 加密模式 (TG 服务器) */
        uint8_t *remote_decrypt_buf = NULL;
        uint32_t decrypt_size = 0;

        crypto_aes_remote_decrypt(data_ptr, length,
                                  &remote_decrypt_buf, &decrypt_size);

        if (ctrl_type == 0x96)
        {
            /* 密钥交换: 用 Local Key 再解密一次, 获取 Remote Key/IV */
            uint8_t *local_decrypt_buf = NULL;
            uint32_t local_decrypt_size = 0;

            crypto_aes_local_decrypt(data_ptr, length,
                                     &local_decrypt_buf, &local_decrypt_size);
            crypto_remote_parse(local_decrypt_buf);
            free(local_decrypt_buf);

            /* 通知 TCP 服务: 密钥交换完成 */
            tcp_service_tg_key_exchange_done();
            ccu_poll_status_reset();
        }

        free(remote_decrypt_buf);
    }
    else
    {
        /* 明文模式 (自定义 TCP): 数据直接可用, 不需要解密 */
        /* 当前 Modbus 产品自定义 TCP 模式下无需处理服务器下发指令 */
        ESP_LOGI(TAG, "Custom TCP plain data received, ctrl=0x%02X", ctrl_type);
    }
}

unsigned char wifi_get_queue_total_data(void)
{
    return (wifi_queue_in != wifi_queue_out) ? 1 : 0;
}

/**
 * @brief  从环形队列读取一个字节
 *
 * [BUG7 FIX] 原代码先做回绕检查再读取 (*wifi_queue_out++),
 *   当 out 恰好在缓冲区末尾时, 会先回绕到头部然后从头部读,
 *   跳过了末尾那个字节的数据.
 *   修复: 先读取当前 out 位置的数据, 再前进指针, 再做回绕检查.
 */
unsigned char wifi_queue_read_byte(void)
{
    unsigned char value = 0;

    if (wifi_queue_out != wifi_queue_in)
    {
        value = *wifi_queue_out;
        wifi_queue_out++;
        if (wifi_queue_out >= (unsigned char *)(wifi_uart_rx_buf + sizeof(wifi_uart_rx_buf)))
        {
            wifi_queue_out = (unsigned char *)(wifi_uart_rx_buf);
        }
    }
    return value;
}