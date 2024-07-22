#include <stdio.h>
#include "heater_uart.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "string.h"

static const int RX_BUF_SIZE = 512;
uint8_t heater_uart_rx_buf_temp[512] = {0};

#define TXD_PIN (GPIO_NUM_21)
#define RXD_PIN (GPIO_NUM_20)

void heater_uart_interface_init(void) 
{
    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_0, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

static void heater_uart_rx_task(void *arg)
{
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_0, heater_uart_rx_buf_temp, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) 
        {
            //printf("heater_uart_rx_task input %d\r\n", rxBytes);
            ESP_LOG_BUFFER_HEXDUMP("rx-task", heater_uart_rx_buf_temp, rxBytes, ESP_LOG_INFO);
            heater_recv_buffer(heater_uart_rx_buf_temp,rxBytes);
        }
    }
}

void heter_uart_init(void)
{
    heater_uart_interface_init();
    heater_uart_service_init();
    xTaskCreate(heater_uart_rx_task, "heater_uart_rx_task", 4096, NULL, 4, NULL);
}

