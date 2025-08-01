#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "esp_log.h"
#include "radio.h"
#include "radio_encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define TAG "radio_queue"

static QueueHandle_t message_queue;

typedef struct
{
    char    data_ptr[253];   /* 数据块首地址 */
    uint8_t data_size;   /* 数据块大小   */
    uint8_t need_ack;    /* 需要应答   */
    uint8_t parameter;   /* 数据参数   */
}message_data_t;

void lora_tx_enqueue(unsigned char *data,uint8_t length,uint8_t need_ack,uint8_t parameter)
{
    message_data_t msg_ptr = {0};
    memcpy(&msg_ptr.data_ptr,data,length < 253 ? length : 253);
    msg_ptr.data_size = length; /* 数据块的长度 */
    msg_ptr.parameter = parameter;
    msg_ptr.need_ack = need_ack;
    xQueueSend(message_queue, (void *)&msg_ptr, ( TickType_t ) 0);
}

void msg_queue_send_task_entry(void *parameter)
{
    message_data_t message_data;
    while(1)
    {
        if (xQueueReceive(message_queue, &message_data, portMAX_DELAY) != pdPASS) 
        {
            ESP_LOGE(TAG,"Queue receive error");
        } 
        else 
        {
            ESP_LOGI(TAG,"message_data len:%d\n",message_data.data_size);
            ESP_LOG_BUFFER_HEX(TAG, (const void *)&message_data.data_ptr, message_data.data_size);
            if (LoRaSend((uint8_t *)&message_data.data_ptr, message_data.data_size, SX126x_TXMODE_ASYNC) == false) 
            {
                ESP_LOGE(TAG,"LoRaSend fail");
            }
            else
            {
                ESP_LOGI(TAG, "%d byte packet sent:", message_data.data_size);
            }
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

void radio_send_queue_init(void)
{
    message_queue = xQueueCreate(5, sizeof(message_data_t));
    if (message_queue == NULL) {
        ESP_LOGE(TAG,"Queue creation failed\n");
        return;
    }

    xTaskCreate(&msg_queue_send_task_entry, "msg_queue_send_task", 4096, NULL, 6, NULL);
}
