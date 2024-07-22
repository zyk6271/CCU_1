/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-08-03     Rick       the first version
 */
#ifndef __CRYPTO_AES_H_
#define __CRYPTO_AES_H_

#include "stdint.h"

void crypto_initialize(void);
void crypto_remote_parse(const unsigned char value[]);
int crypto_aes_local_encrypt(uint8_t* plain_buffer, int plain_size, uint8_t** output, int *output_length);
int crypto_aes_local_decrypt(uint8_t *decrypt_buffer,int decrypt_size,uint8_t **output,int *output_length);
int crypto_aes_remote_encrypt(uint8_t* plain_buffer, int plain_size, uint8_t** output, int *output_length);
int crypto_aes_remote_decrypt(uint8_t *decrypt_buffer,int decrypt_size,uint8_t **output,int *output_length);

#endif
