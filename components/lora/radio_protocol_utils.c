/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-03-28     Rick       the first version
 */
#include "radio_protocol_utils.h"
#include <string.h>
#include <stdint.h>

volatile unsigned char lora_tx_buf[255];         //LoRa发送缓存

uint8_t crc8_le_calc(uint8_t* buf, size_t length) 
{
    unsigned char crc = 0x00;
    for (int i = 0; i < length; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

unsigned char get_check_crc(unsigned char *pack, unsigned short pack_len)
{
    //unsigned char calc_crc = HAL_CRC_Calculate(&crc_handle, (uint32_t *)pack, pack_len);

    unsigned char calc_crc = crc8_le_calc(pack, pack_len);

    return calc_crc;
}

unsigned char *get_lora_tx_buf(void)
{
    return lora_tx_buf;
}

/**
 * @brief  写单字节
 * @param[in] {dest} 缓存区地址偏移
 * @param[in] {byte} 写入字节值
 * @return 写入完成后的总长度
 */
unsigned short set_lora_tx_byte(unsigned short dest, unsigned char byte)
{
    unsigned char *obj = (unsigned char *)lora_tx_buf + dest;

    *obj = byte;
    dest += 1;

    return dest;
}

/**
 * @brief  写wifi_uart_buffer
 * @param[in] {dest} 缓存区地址偏移
 * @param[in] {src} 源地址（需要发送的数据）
 * @param[in] {len} 需要发送的数据长度
 * @return 写入完成后的总长度
 */
unsigned short set_lora_tx_buffer(unsigned short dest, const unsigned char *src, unsigned short len)
{
    unsigned char *obj = (unsigned char *)lora_tx_buf + dest;

    memcpy(obj,src,len);

    dest += len;
    return dest;
}

/**
 * @brief  写4字节
 * @param[in] {dest} 缓存区地址偏移
 * @param[in] {byte} 写入字节值
 * @return 写入完成后的总长度
 */
unsigned short set_lora_tx_word(unsigned short dest, unsigned int word)
{
    unsigned char *obj = (unsigned char *)lora_tx_buf + dest;

    *obj = (word >> 24) & 0xFF;
    obj ++;
    *obj = (word >> 16) & 0xFF;
    obj ++;
    *obj = (word >> 8) & 0xFF;
    obj ++;
    *obj = word & 0xFF;

    dest += 4;

    return dest;
}

/**
 * @param[in] {dest} 缓存区地址偏移
 * @param[in] {src} 源地址（需要发送的数据）
 * @param[in] {len} 需要发送的数据长度
 * @return 写入完成后的总长度
 */
unsigned short set_lora_tx_crc(unsigned short dest)
{
    unsigned char *obj = (unsigned char *)lora_tx_buf + dest;
    *obj = crc8_le_calc(lora_tx_buf, dest);
    dest += 1;

    return dest;
}
