// #include <stdio.h>
// #include <string.h>
// #include <stdint.h>
// #include "esp_log.h"
// #include "radio.h"
// #include "radio_encoder.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/queue.h"

// #define TAG "radio_queue"

// static QueueHandle_t message_queue;

// typedef struct
// {
//     char    data_ptr[253];   /* 数据块首地址 */
//     uint8_t data_size;   /* 数据块大小   */
//     uint8_t need_ack;    /* 需要应答   */
//     uint8_t parameter;   /* 数据参数   */
// }message_data_t;

// void lora_tx_enqueue(unsigned char *data,uint8_t length,uint8_t need_ack,uint8_t parameter)
// {
//     message_data_t msg_ptr = {0};
//     memcpy(&msg_ptr.data_ptr,data,length < 253 ? length : 253);
//     msg_ptr.data_size = length; /* 数据块的长度 */
//     msg_ptr.parameter = parameter;
//     msg_ptr.need_ack = need_ack;
//     xQueueSend(message_queue, (void *)&msg_ptr, ( TickType_t ) 0);
// }

// void msg_queue_send_task_entry(void *parameter)
// {
//     message_data_t message_data;
//     while(1)
//     {
//         if (xQueueReceive(message_queue, &message_data, portMAX_DELAY) != pdPASS) 
//         {
//             ESP_LOGE(TAG,"Queue receive error");
//         } 
//         else 
//         {
//             ESP_LOGI(TAG,"message_data len:%d\n",message_data.data_size);
//             ESP_LOG_BUFFER_HEX(TAG, (const void *)&message_data.data_ptr, message_data.data_size);
//             lora_send_preamble_length_set(message_data.parameter);
//             if (LoRaSend((uint8_t *)&message_data.data_ptr, message_data.data_size, SX126x_TXMODE_ASYNC) == false) 
//             {
//                 ESP_LOGE(TAG,"LoRaSend fail");
//             }
//             else
//             {
//                 ESP_LOGI(TAG, "%d byte packet sent:", message_data.data_size);
//             }
//             if(message_data.parameter)
//             {
//                 vTaskDelay(pdMS_TO_TICKS(1800));
//             }
//             else
//             {
//                 vTaskDelay(pdMS_TO_TICKS(200));
//             }
//         }
//     }
// }

// void radio_send_queue_init(void)
// {
//     message_queue = xQueueCreate(5, sizeof(message_data_t));
//     if (message_queue == NULL) {
//         ESP_LOGE(TAG,"Queue creation failed\n");
//         return;
//     }

//     xTaskCreate(&msg_queue_send_task_entry, "msg_queue_send_task", 4096, NULL, 6, NULL);
// }

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "esp_log.h"
#include "radio.h"
#include "radio_encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#define TAG "radio_queue"

// 命令类型定义
typedef enum {
    CMD_NORMAL = 0,     // 普通数据包
    CMD_BROADCAST = 1   // 广播包（CAD长前导码）
} command_type_t;

static QueueHandle_t message_queue;

typedef struct {
    char    data_ptr[253];   /* 数据块首地址 */
    uint8_t data_size;       /* 数据块大小   */
    uint8_t need_ack;        /* 需要应答   */
    uint8_t parameter;       /* 数据参数   */
} message_data_t;

// 全局变量记录待发送的广播包
static message_data_t pending_broadcast = {0};
static bool has_pending_broadcast = false;
static SemaphoreHandle_t broadcast_mutex = NULL;

// 广播包计时相关变量
static TickType_t broadcast_ready_time = 0;  // 广播包可以发送的时间点
static bool broadcast_waiting = false;       // 广播包是否在等待
static const TickType_t BROADCAST_DELAY_MS = 5000; // 广播包等待5秒

void lora_tx_enqueue(unsigned char *data, uint8_t length, uint8_t need_ack, uint8_t parameter)
{
    message_data_t msg_ptr = {0};
    
    if (length >= 253) {
        ESP_LOGE(TAG, "Data too long: %d", length);
        return;
    }
    
    memcpy(msg_ptr.data_ptr, data, length);
    msg_ptr.data_size = length;
    msg_ptr.parameter = parameter;
    msg_ptr.need_ack = need_ack;
    
    if (parameter == 1) {
        // 广播包处理：替换旧的广播包
        if (xSemaphoreTake(broadcast_mutex, portMAX_DELAY) == pdTRUE) {
            memcpy(&pending_broadcast, &msg_ptr, sizeof(message_data_t));
            has_pending_broadcast = true;
            broadcast_waiting = true;
            
            // 设置广播包可以发送的时间点（当前时间 + 5秒）
            broadcast_ready_time = xTaskGetTickCount() + pdMS_TO_TICKS(BROADCAST_DELAY_MS);
            
            xSemaphoreGive(broadcast_mutex);
            ESP_LOGI(TAG, "Broadcast packet updated, length: %d, waiting 5s", length);
        }
    } else {
        // 普通数据包正常入队
        if (xQueueSend(message_queue, (void *)&msg_ptr, (TickType_t)0) != pdTRUE) {
            ESP_LOGE(TAG, "Normal packet queue full, drop packet");
        } else {
            ESP_LOGI(TAG, "Normal packet enqueued, length: %d", length);
            
            // 如果有广播包在等待，重置等待时间
            if (xSemaphoreTake(broadcast_mutex, portMAX_DELAY) == pdTRUE) {
                if (broadcast_waiting) {
                    broadcast_ready_time = xTaskGetTickCount() + pdMS_TO_TICKS(BROADCAST_DELAY_MS);
                    ESP_LOGI(TAG, "Broadcast delay reset due to normal packet");
                }
                xSemaphoreGive(broadcast_mutex);
            }
        }
    }
}

// 检查广播包是否可以发送
static bool is_broadcast_ready(void)
{
    if (xSemaphoreTake(broadcast_mutex, portMAX_DELAY) == pdTRUE) {
        bool ready = (has_pending_broadcast && 
                     broadcast_waiting && 
                     (xTaskGetTickCount() >= broadcast_ready_time));
        xSemaphoreGive(broadcast_mutex);
        return ready;
    }
    return false;
}

// 检查并获取待发送的广播包
static bool get_pending_broadcast(message_data_t *msg_out)
{
    if (xSemaphoreTake(broadcast_mutex, portMAX_DELAY) == pdTRUE) {
        if (has_pending_broadcast && broadcast_waiting && (xTaskGetTickCount() >= broadcast_ready_time)) {
            memcpy(msg_out, &pending_broadcast, sizeof(message_data_t));
            has_pending_broadcast = false; // 取走后清除
            broadcast_waiting = false;
            xSemaphoreGive(broadcast_mutex);
            return true;
        }
        xSemaphoreGive(broadcast_mutex);
    }
    return false;
}

// 获取广播包剩余等待时间（用于调试）
static TickType_t get_broadcast_remaining_time(void)
{
    if (xSemaphoreTake(broadcast_mutex, portMAX_DELAY) == pdTRUE) {
        TickType_t current_time = xTaskGetTickCount();
        TickType_t remaining = 0;
        
        if (broadcast_waiting && broadcast_ready_time > current_time) {
            remaining = broadcast_ready_time - current_time;
        }
        
        xSemaphoreGive(broadcast_mutex);
        return remaining;
    }
    return 0;
}

void msg_queue_send_task_entry(void *parameter)
{
    message_data_t message_data;
    
    while(1) {
        bool packet_sent = false;
        
        // 第一步：优先发送普通数据包
        if (xQueueReceive(message_queue, &message_data, 0) == pdTRUE) {
            ESP_LOGI(TAG, "Sending normal packet, len:%d", message_data.data_size);
            ESP_LOG_BUFFER_HEX(TAG, (const void *)message_data.data_ptr, message_data.data_size);
            
            lora_send_preamble_length_set(message_data.parameter);
            if (LoRaSend((uint8_t *)message_data.data_ptr, message_data.data_size, SX126x_TXMODE_ASYNC) == false) {
                ESP_LOGE(TAG, "LoRaSend normal packet fail");
            } else {
                ESP_LOGI(TAG, "%d byte normal packet sent", message_data.data_size);
                packet_sent = true;
            }
            
            // 普通包发送后短延时
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // 第二步：检查广播包是否可以发送
        if (!packet_sent && is_broadcast_ready()) {
            // 获取并发送广播包
            if (get_pending_broadcast(&message_data)) {
                ESP_LOGI(TAG, "Sending broadcast packet, len:%d", message_data.data_size);
                ESP_LOG_BUFFER_HEX(TAG, (const void *)message_data.data_ptr, message_data.data_size);
                
                lora_send_preamble_length_set(message_data.parameter);
                if (LoRaSend((uint8_t *)message_data.data_ptr, message_data.data_size, SX126x_TXMODE_ASYNC) == false) {
                    ESP_LOGE(TAG, "LoRaSend broadcast packet fail");
                } else {
                    ESP_LOGI(TAG, "%d byte broadcast packet sent", message_data.data_size);
                    packet_sent = true;
                }
                
                // 广播包发送后长延时
                vTaskDelay(pdMS_TO_TICKS(1700));
            }
        }
        
        // 第三步：如果没有任何包发送，等待一段时间再检查
        if (!packet_sent) {
            // 如果有广播包在等待，显示剩余时间（调试用）
            // if (xSemaphoreTake(broadcast_mutex, 0) == pdTRUE) {
            //     if (broadcast_waiting) {
            //         TickType_t remaining = get_broadcast_remaining_time();
            //         if (remaining > 0) {
            //             ESP_LOGD(TAG, "Broadcast waiting, remaining: %lu ms", 
            //                     pdTICKS_TO_MS(remaining));
            //         }
            //     }
            //     xSemaphoreGive(broadcast_mutex);
            // }
            
            vTaskDelay(pdMS_TO_TICKS(100)); // 100ms检查间隔
        }
    }
}

void radio_send_queue_init(void)
{
    // 创建消息队列
    message_queue = xQueueCreate(5, sizeof(message_data_t));
    if (message_queue == NULL) {
        ESP_LOGE(TAG, "Queue creation failed");
        return;
    }
    
    // 创建广播包互斥锁
    broadcast_mutex = xSemaphoreCreateMutex();
    if (broadcast_mutex == NULL) {
        ESP_LOGE(TAG, "Mutex creation failed");
        return;
    }
    
    // 初始化广播包状态
    has_pending_broadcast = false;
    broadcast_waiting = false;
    broadcast_ready_time = 0;
    
    // 创建发送任务
    xTaskCreate(&msg_queue_send_task_entry, "msg_queue_send_task", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "Radio send queue initialized");
}