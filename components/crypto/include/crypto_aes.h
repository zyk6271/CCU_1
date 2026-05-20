/**
 * @file crypto_aes.h
 * @brief AES-256-CBC 加解密接口
 */

#ifndef CRYPTO_AES_H_
#define CRYPTO_AES_H_

#include <stdint.h>

#define CRYPTO_ENABLED  1

/**
 * @brief 初始化 AES context, 从 NVS 加载本地密钥
 */
void crypto_initialize(void);

/**
 * @brief 解析服务器下发的远端密钥和 IV
 * @param value 49字节: [0]=保留, [1~32]=Key, [33~48]=IV
 */
void crypto_remote_parse(const unsigned char value[]);

/**
 * @brief 本地密钥加密 (Local Key + Local IV)
 * @param plain_buffer  明文数据
 * @param plain_size    明文长度
 * @param output        输出密文指针 (调用方负责 free)
 * @param output_length 输出密文长度
 */
int crypto_aes_local_encrypt(const uint8_t *plain_buffer, int plain_size,
                              uint8_t **output, uint32_t *output_length);

/**
 * @brief 本地密钥解密
 * @param decrypt_buffer 密文数据 (支持 volatile 缓冲区)
 * @param decrypt_size   密文长度 (必须是 16 的倍数)
 * @param output         输出明文指针 (调用方负责 free)
 * @param output_length  输出明文长度
 */
int crypto_aes_local_decrypt(const volatile uint8_t *decrypt_buffer, int decrypt_size,
                              uint8_t **output, uint32_t *output_length);

/**
 * @brief 远端密钥加密 (Remote Key + Remote IV)
 */
int crypto_aes_remote_encrypt(const uint8_t *plain_buffer, int plain_size,
                               uint8_t **output, uint32_t *output_length);

/**
 * @brief 远端密钥解密
 * @param decrypt_buffer 密文数据 (支持 volatile 缓冲区)
 */
int crypto_aes_remote_decrypt(const volatile uint8_t *decrypt_buffer, int decrypt_size,
                               uint8_t **output, uint32_t *output_length);

#endif /* CRYPTO_AES_H_ */