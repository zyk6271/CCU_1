/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-08-03     Rick       the first version
 */
#include "freertos/FreeRTOS.h"
#include "aes/esp_aes.h"
#include "esp_log.h"

#include "string.h"
#include "crypto_aes.h"
#include "storage.h"

static const char *TAG = "crypto";

esp_aes_context aes_handle;

uint8_t Local_AES_Key[32] = {0};

static uint8_t Local_AES_IV[16] =
{
    0xBB, 0x68, 0xE4, 0xC5, 0x5B, 0xD3, 0x0C, 0x83,
    0x3C, 0x15, 0x8F, 0x74, 0xB1, 0x67, 0xCB, 0xF9
};

static uint8_t Remote_AES_Key[32] = {0};
static uint8_t Remote_AES_IV[16] = {0};

void crypto_initialize(void)
{
    uint8_t local_key[32];
    uint32_t key_length;
    esp_aes_init(&aes_handle);
    if(storage_read_key_blob("app_key",local_key,&key_length) == ESP_OK)
    {
        memcpy(Local_AES_Key,local_key,32);
    }
}

void crypto_remote_parse(const unsigned char value[])
{
    memcpy(Remote_AES_Key, &value[1], 32);
    memcpy(Remote_AES_IV, &value[33], 16);
    ESP_LOGI(TAG,"crypto_remote_parse");
}

int crypto_aes_local_encrypt(uint8_t* plain_buffer, int plain_size, uint8_t** output, uint32_t *output_length)
{
    int result = 0;

    int remainder = plain_size % 16;
    int paddedSize = plain_size + (16 - remainder);

    uint8_t* encrypted = (uint8_t*) malloc((paddedSize) * sizeof(uint8_t));

    uint8_t iv[16] = {0};
    memcpy(iv,Local_AES_IV,16);

    uint8_t dataBuffer[paddedSize];
    memcpy(dataBuffer, plain_buffer, plain_size);

    esp_aes_setkey(&aes_handle, Local_AES_Key, 256);

    if (remainder > 0) {
        // 填充剩余部分
        for (int i = plain_size; i < paddedSize; i++)
        {
            dataBuffer[i] = 0;
        }
    }

    result = esp_aes_crypt_cbc(&aes_handle, ESP_AES_ENCRYPT, paddedSize, iv, dataBuffer, encrypted);
    *output = encrypted;
    *output_length = paddedSize;

    return result;
}

int crypto_aes_local_decrypt(uint8_t *decrypt_buffer,int decrypt_size,uint8_t **output,uint32_t *output_length)
{
    int result = 0;

    uint8_t iv[16] = {0};
    memcpy(iv,Local_AES_IV,16);

    uint8_t* decrypted = (uint8_t*) malloc((decrypt_size) * sizeof(uint8_t));

    esp_aes_setkey(&aes_handle, Local_AES_Key, sizeof(Local_AES_Key));

    result = esp_aes_crypt_cbc(&aes_handle, ESP_AES_DECRYPT, decrypt_size, iv, decrypt_buffer, decrypted);
    *output = decrypted;
    *output_length = decrypt_size;

    return result;
}

int crypto_aes_remote_encrypt(uint8_t* plain_buffer, int plain_size, uint8_t** output, uint32_t *output_length)
{
    int result = 0;

    int remainder = plain_size % 16;
    int paddedSize = plain_size + (16 - remainder);

    uint8_t* encrypted = (uint8_t*) malloc((paddedSize) * sizeof(uint8_t));

    uint8_t iv[16] = {0};
    memcpy(iv,Remote_AES_IV,16);

    uint8_t dataBuffer[paddedSize];
    memcpy(dataBuffer, plain_buffer, plain_size);

    esp_aes_setkey(&aes_handle, Remote_AES_Key, 256);

    if (remainder > 0) {
        // 填充剩余部分
        for (int i = plain_size; i < paddedSize; i++)
        {
            dataBuffer[i] = 0;
        }
    }

    result = esp_aes_crypt_cbc(&aes_handle, ESP_AES_ENCRYPT, paddedSize, iv, dataBuffer, encrypted);
    *output = encrypted;
    *output_length = paddedSize;

    return result;
}

int crypto_aes_remote_decrypt(uint8_t *decrypt_buffer,int decrypt_size,uint8_t **output,uint32_t *output_length)
{
    int result = 0;

    uint8_t iv[16] = {0};
    memcpy(iv,Remote_AES_IV,16);

    uint8_t* decrypted = (uint8_t*) malloc((decrypt_size) * sizeof(uint8_t));

    esp_aes_setkey(&aes_handle, Remote_AES_Key, sizeof(Local_AES_Key));

    result = esp_aes_crypt_cbc(&aes_handle, ESP_AES_DECRYPT, decrypt_size, iv, decrypt_buffer, decrypted);
    *output = decrypted;
    *output_length = decrypt_size;

    return result;
}