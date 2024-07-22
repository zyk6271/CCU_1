#include <stdio.h>
#include "led.h"
#include "signal_led.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

led_t *led_network_status =  NULL;
led_mem_opreation_t led_mem_opreation;

#define LED_NETWORK_PIN    GPIO_NUM_4
#define LED_NETWORK_PIN_SEL  ((1ULL<<LED_NETWORK_PIN))

void led_network_switch_on(void *param)
{
    gpio_set_level(LED_NETWORK_PIN,0);
}

void led_network_switch_off(void *param)
{
    gpio_set_level(LED_NETWORK_PIN,1);
}

void led_network_status_handle(uint8_t type)
{
    switch(type)
    {
    case 0://启动失败
        led_stop(led_network_status);
        break;
    case 1://AP慢闪,等待配网
        led_stop(led_network_status);
        led_set_mode(led_network_status, LOOP_PERMANENT,"1000,1000,");
        led_start(led_network_status);
        break;
    case 2://连接路由器中，快闪
        led_stop(led_network_status);
        led_set_mode(led_network_status, LOOP_PERMANENT,"150,150,");
        led_start(led_network_status);
        break;
    case 3://已连接互联网，长亮
        led_stop(led_network_status);
        led_set_mode(led_network_status, LOOP_PERMANENT,"200,0,");
        led_start(led_network_status);
        break;
    default:
        break;
    }
}

static void led_run(void *parameter)
{
    while(1)
    {
        led_ticks();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

int signal_led_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = LED_NETWORK_PIN_SEL;
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);

    led_network_status = led_create(led_network_switch_on, led_network_switch_off, NULL);
    led_mem_opreation.malloc_fn = (void* (*)(size_t))malloc;
    led_mem_opreation.free_fn = free;
    led_set_mem_operation(&led_mem_opreation);

    xTaskCreatePinnedToCore(led_run, "led", 1024, NULL, 3, NULL, tskNO_AFFINITY);
    
    return 0;
}