#include <stdio.h>
#include "esp_log.h"
#include "driver/uart.h"

#define UART_NUM UART_NUM_1  // Use UART1
#define BUF_SIZE 1024        // Buffer size for incoming data
#define RX_PIN 12            // GPIO pin for UART RX
#define TX_PIN 13            // GPIO pin for UART TX
#define PREAMBLE_1 0x55
#define PREAMBLE_2 0xAA

static const char *TAG = "UART_POLLING";

void uart_polling_task(void *pvParameters) {
    uint8_t data[BUF_SIZE];

    while (1) {
        // Read bytes from UART
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE * 2, 10 / portTICK_PERIOD_MS);
        ESP_LOG_BUFFER_HEX(TAG, data, 5);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Hello, world!");

    // Configure UART
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    // Install UART driver without an event queue
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "UART configured");

    // Create the polling task
    xTaskCreate(uart_polling_task, "uart_polling_task", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "UART polling task initialized.");
}
