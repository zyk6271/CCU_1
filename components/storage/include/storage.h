/**
 * @file storage.h
 * @brief NVS 持久化存储接口
 */

#ifndef STORAGE_H_
#define STORAGE_H_

#include "esp_system.h"
#include <stdint.h>

esp_err_t storage_init(void);

esp_err_t storage_save_key_blob(const char *key, const uint8_t *value, uint32_t length);
esp_err_t storage_read_key_blob(const char *key, uint8_t *value, uint32_t *length);

esp_err_t storage_save_key_value(const char *key, uint32_t value);
esp_err_t storage_read_key_value(const char *key, uint32_t *value);

esp_err_t storage_erase_key(const char *key);
esp_err_t storage_erase_all(void);

#endif /* STORAGE_H_ */
