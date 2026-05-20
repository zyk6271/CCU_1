/**
 * @file crypto_aes.c
 * @brief AES-256-CBC 加解密实现
 */

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "aes/esp_aes.h"
#include "esp_log.h"

#include "crypto_aes.h"
#include "storage.h"

#define TAG "crypto"

#define AES_KEY_SIZE   32
#define AES_BLOCK_SIZE 16
#define AES_KEY_BITS   256

/* ============================================================ */
/* 密钥与 IV 存储                                                 */
/* ============================================================ */

static esp_aes_context s_aes_ctx;

uint8_t Local_AES_Key[AES_KEY_SIZE] = {0};

static const uint8_t s_local_iv[AES_BLOCK_SIZE] =
{
    0xBB, 0x68, 0xE4, 0xC5, 0x5B, 0xD3, 0x0C, 0x83,
    0x3C, 0x15, 0x8F, 0x74, 0xB1, 0x67, 0xCB, 0xF9
};

static uint8_t s_remote_key[AES_KEY_SIZE]  = {0};
static uint8_t s_remote_iv[AES_BLOCK_SIZE] = {0};

/* ============================================================ */
/* 初始化                                                         */
/* ============================================================ */

void crypto_initialize(void)
{
    uint8_t local_key[AES_KEY_SIZE];
    uint32_t key_length = AES_KEY_SIZE;

    esp_aes_init(&s_aes_ctx);

    if (storage_read_key_blob("app_key", local_key, &key_length) == ESP_OK &&
        key_length == AES_KEY_SIZE)
    {
        memcpy(Local_AES_Key, local_key, AES_KEY_SIZE);
        ESP_LOGI(TAG, "Local AES key loaded from NVS");
    }
    else
    {
        ESP_LOGW(TAG, "Local AES key not found in NVS");
    }
}

/* ============================================================ */
/* 远端密钥解析                                                   */
/* ============================================================ */

void crypto_remote_parse(const unsigned char value[])
{
    /* value 布局: [0]=保留, [1~32]=Key(32B), [33~48]=IV(16B) */
    memcpy(s_remote_key, &value[1],  AES_KEY_SIZE);
    memcpy(s_remote_iv,  &value[33], AES_BLOCK_SIZE);
    ESP_LOGI(TAG, "Remote AES key/IV updated");
}

/* ============================================================ */
/* 内部工具: 计算 PKCS#0 对齐后的大小 (零填充)                     */
/* ============================================================ */

static int aes_padded_size(int plain_size)
{
    int remainder = plain_size % AES_BLOCK_SIZE;
    if (remainder == 0)
    {
        return plain_size;
    }
    return plain_size + (AES_BLOCK_SIZE - remainder);
}

/* ============================================================ */
/* 本地密钥加密                                                   */
/* ============================================================ */

int crypto_aes_local_encrypt(const uint8_t *plain_buffer, int plain_size,
                              uint8_t **output, uint32_t *output_length)
{
#if CRYPTO_ENABLED == 1
    int padded_size = aes_padded_size(plain_size);

    uint8_t *encrypted = (uint8_t *)malloc(padded_size);
    if (!encrypted)
    {
        return -1;
    }

    /* 拷贝明文并零填充 */
    memcpy(encrypted, plain_buffer, plain_size);
    memset(encrypted + plain_size, 0, padded_size - plain_size);

    uint8_t iv[AES_BLOCK_SIZE];
    memcpy(iv, s_local_iv, AES_BLOCK_SIZE);

    esp_aes_setkey(&s_aes_ctx, Local_AES_Key, AES_KEY_BITS);

    int result = esp_aes_crypt_cbc(&s_aes_ctx, ESP_AES_ENCRYPT,
                                   padded_size, iv, encrypted, encrypted);
    if (result != 0)
    {
        free(encrypted);
        return result;
    }

    *output = encrypted;
    *output_length = (uint32_t)padded_size;
    return 0;

#else
    uint8_t *buf = (uint8_t *)malloc(plain_size);
    if (!buf)
    {
        return -1;
    }
    memcpy(buf, plain_buffer, plain_size);
    *output = buf;
    *output_length = (uint32_t)plain_size;
    return 0;
#endif
}

/* ============================================================ */
/* 本地密钥解密                                                   */
/* ============================================================ */

int crypto_aes_local_decrypt(const volatile uint8_t *decrypt_buffer, int decrypt_size,
                              uint8_t **output, uint32_t *output_length)
{
#if CRYPTO_ENABLED == 1
    uint8_t *decrypted = (uint8_t *)malloc(decrypt_size);
    if (!decrypted)
    {
        return -1;
    }

    /* 将 volatile 缓冲区拷贝到普通内存后再传给 AES 引擎 */
    uint8_t *in_buf = (uint8_t *)malloc(decrypt_size);
    if (!in_buf)
    {
        free(decrypted);
        return -1;
    }
    memcpy(in_buf, (const uint8_t *)decrypt_buffer, decrypt_size);

    uint8_t iv[AES_BLOCK_SIZE];
    memcpy(iv, s_local_iv, AES_BLOCK_SIZE);

    esp_aes_setkey(&s_aes_ctx, Local_AES_Key, AES_KEY_BITS);

    int result = esp_aes_crypt_cbc(&s_aes_ctx, ESP_AES_DECRYPT,
                                   decrypt_size, iv, in_buf, decrypted);
    free(in_buf);

    if (result != 0)
    {
        free(decrypted);
        return result;
    }

    *output = decrypted;
    *output_length = (uint32_t)decrypt_size;
    return 0;

#else
    uint8_t *buf = (uint8_t *)malloc(decrypt_size);
    if (!buf)
    {
        return -1;
    }
    memcpy(buf, (const uint8_t *)decrypt_buffer, decrypt_size);
    *output = buf;
    *output_length = (uint32_t)decrypt_size;
    return 0;
#endif
}

/* ============================================================ */
/* 远端密钥加密                                                   */
/* ============================================================ */

int crypto_aes_remote_encrypt(const uint8_t *plain_buffer, int plain_size,
                               uint8_t **output, uint32_t *output_length)
{
#if CRYPTO_ENABLED == 1
    int padded_size = aes_padded_size(plain_size);

    uint8_t *encrypted = (uint8_t *)malloc(padded_size);
    if (!encrypted)
    {
        return -1;
    }

    memcpy(encrypted, plain_buffer, plain_size);
    memset(encrypted + plain_size, 0, padded_size - plain_size);

    uint8_t iv[AES_BLOCK_SIZE];
    memcpy(iv, s_remote_iv, AES_BLOCK_SIZE);

    esp_aes_setkey(&s_aes_ctx, s_remote_key, AES_KEY_BITS);

    int result = esp_aes_crypt_cbc(&s_aes_ctx, ESP_AES_ENCRYPT,
                                   padded_size, iv, encrypted, encrypted);
    if (result != 0)
    {
        free(encrypted);
        return result;
    }

    *output = encrypted;
    *output_length = (uint32_t)padded_size;
    return 0;

#else
    uint8_t *buf = (uint8_t *)malloc(plain_size);
    if (!buf)
    {
        return -1;
    }
    memcpy(buf, plain_buffer, plain_size);
    *output = buf;
    *output_length = (uint32_t)plain_size;
    return 0;
#endif
}

/* ============================================================ */
/* 远端密钥解密                                                   */
/* ============================================================ */

int crypto_aes_remote_decrypt(const volatile uint8_t *decrypt_buffer, int decrypt_size,
                               uint8_t **output, uint32_t *output_length)
{
#if CRYPTO_ENABLED == 1
    uint8_t *decrypted = (uint8_t *)malloc(decrypt_size);
    if (!decrypted)
    {
        return -1;
    }

    uint8_t *in_buf = (uint8_t *)malloc(decrypt_size);
    if (!in_buf)
    {
        free(decrypted);
        return -1;
    }
    memcpy(in_buf, (const uint8_t *)decrypt_buffer, decrypt_size);

    uint8_t iv[AES_BLOCK_SIZE];
    memcpy(iv, s_remote_iv, AES_BLOCK_SIZE);

    /* 修复原始 bug: 原代码用 sizeof(Local_AES_Key) 做远端密钥位宽, 应为 AES_KEY_BITS */
    esp_aes_setkey(&s_aes_ctx, s_remote_key, AES_KEY_BITS);

    int result = esp_aes_crypt_cbc(&s_aes_ctx, ESP_AES_DECRYPT,
                                   decrypt_size, iv, in_buf, decrypted);
    free(in_buf);

    if (result != 0)
    {
        free(decrypted);
        return result;
    }

    *output = decrypted;
    *output_length = (uint32_t)decrypt_size;
    return 0;

#else
    uint8_t *buf = (uint8_t *)malloc(decrypt_size);
    if (!buf)
    {
        return -1;
    }
    memcpy(buf, (const uint8_t *)decrypt_buffer, decrypt_size);
    *output = buf;
    *output_length = (uint32_t)decrypt_size;
    return 0;
#endif
}