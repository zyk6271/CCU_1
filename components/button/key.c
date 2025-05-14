#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "button.h"
#include "key.h"
#include "led.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "wifi_manager.h"

static const char *TAG = "key";

Button_t reset_key;

#define RST_KEY_PIN    GPIO_NUM_5
#define RST_KEY_PIN_SEL  ((1ULL<<RST_KEY_PIN))

static uint8_t reset_key_long_cnt = 0;
static uint8_t reset_key_long_click = 0;

uint8_t reset_key_level_get_level(void)
{
    return gpio_get_level(RST_KEY_PIN);
}

void reset_key_long_press_callback(void *parameter)
{
    if(reset_key_long_cnt < 40)
    {
        ESP_LOGI(TAG,"reset_key_long_cnt increase %d",reset_key_long_cnt);
        reset_key_long_cnt++;
    }
    else
    {
        if(reset_key_long_click == 0)
        {
            reset_key_long_click = 1;
            smartconfig_reset();
            ESP_LOGI(TAG,"reset_key_long_press_click");
        }
    }
}
void reset_key_long_free_callback(void *parameter)
{
    reset_key_long_cnt = 0;
    reset_key_long_click = 0;
    ESP_LOGI(TAG,"reset_key_long_free_callback");
}

void key_thread_callback(void *parameter)
{
    while(1)
    {
        Button_Process();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void key_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = RST_KEY_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    Button_Create("reset_key", &reset_key, reset_key_level_get_level, 0);
    Button_Attach(&reset_key, BUTTON_LONG, reset_key_long_press_callback);
    Button_Attach(&reset_key, BUTTON_LONG_FREE, reset_key_long_free_callback);
    
    xTaskCreatePinnedToCore(key_thread_callback, "key", 2048, NULL, 3, NULL, tskNO_AFFINITY);
}