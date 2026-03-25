#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "storage.h"
#include "esp_log.h"

#define STORAGE_NAMESPACE "storage"
static const char *TAG = "storage";

esp_err_t storage_save_key_blob(char* key,uint8_t* value,uint32_t length)
{
    nvs_handle_t my_handle;
    esp_err_t err = ESP_OK;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "storage_save_key_blob nvs_open failed");
        return err;
    }

    err = nvs_set_blob(my_handle, key, value, length);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "storage_save_key_blob nvs_set_blob failed");
        nvs_close(my_handle);
        return err;
    }

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "storage_save_key_blob nvs_commit failed");
    }

    // Close
    nvs_close(my_handle);

    return err;
}

esp_err_t storage_save_key_value(char* key,uint32_t value)
{
    nvs_handle_t my_handle;
    esp_err_t err = ESP_OK;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "storage_save_key_value nvs_open failed");
        return err;
    }

    err = nvs_set_u32(my_handle, key, value);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "storage_save_key_value nvs_set_u32 failed");
        nvs_close(my_handle);
        return err;
    }

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "storage_save_key_value nvs_commit failed");
    }

    // Close
    nvs_close(my_handle);

    return err;
}

esp_err_t storage_read_key_blob(char* key,uint8_t* value,uint32_t *length)
{
    nvs_handle_t my_handle;
    esp_err_t err = ESP_OK;
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) 
    {
        return err;
    }

    err = nvs_get_blob(my_handle, key, NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) 
    {
        nvs_close(my_handle);
        return err;
    }

    if (required_size == 0) 
    {
        err = ESP_ERR_NVS_NOT_FOUND;
    } 
    else 
    {
        err = nvs_get_blob(my_handle, key, value, &required_size);
        if (err != ESP_OK) 
        {
            nvs_close(my_handle);
            return err;
        }
        *length = required_size;
    }
    // Close
    nvs_close(my_handle);

    return err;
}

esp_err_t storage_read_key_value(char* key,uint32_t *value)
{
    esp_err_t err = ESP_OK;
    nvs_handle_t my_handle;

    uint32_t value_temp = 0;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) 
    {
        return err;
    }

    err = nvs_get_u32(my_handle, key, &value_temp);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) 
    {
        nvs_close(my_handle);
        return err;
    }

    *value = value_temp;

    // Close
    nvs_close(my_handle);

    return err;
}

esp_err_t storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    return err;
}