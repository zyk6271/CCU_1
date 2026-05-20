/**
 * @file storage.c
 * @brief NVS 持久化存储实现
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "storage.h"
#include "esp_log.h"

#define TAG "storage"
#define STORAGE_NAMESPACE "storage"

esp_err_t storage_save_key_blob(const char *key, const uint8_t *value, uint32_t length)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_blob(handle, key, value, length);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t storage_save_key_value(const char *key, uint32_t value)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_u32(handle, key, value);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t storage_read_key_blob(const char *key, uint8_t *value, uint32_t *length)
{
    nvs_handle_t handle;
    esp_err_t err;
    size_t required_size = 0;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_get_blob(handle, key, NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(handle);
        return err;
    }

    if (required_size == 0)
    {
        nvs_close(handle);
        return ESP_ERR_NVS_NOT_FOUND;
    }

    err = nvs_get_blob(handle, key, value, &required_size);
    if (err == ESP_OK)
    {
        *length = (uint32_t)required_size;
    }

    nvs_close(handle);
    return err;
}

esp_err_t storage_read_key_value(const char *key, uint32_t *value)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    uint32_t temp = 0;
    err = nvs_get_u32(handle, key, &temp);
    if (err == ESP_OK)
    {
        *value = temp;
    }

    nvs_close(handle);
    return err;
}

esp_err_t storage_erase_key(const char *key)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_erase_key(handle, key);
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t storage_erase_all(void)
{
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS flash erase failed: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_flash_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS flash init after erase failed: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    return err;
}
