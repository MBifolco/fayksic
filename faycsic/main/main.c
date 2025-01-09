#include "main.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <freertos/queue.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if CONFIG_MOCK_MESSAGES
#include "mock_messages.h"
#endif

#define UART_NUM UART_NUM_1 // Use UART1
#define BUF_SIZE 2048       // Buffer size for incoming data
#define RX_PIN 12           // GPIO pin for UART RX
#define TX_PIN 13           // GPIO pin for UART TX

static const char *TAG = "MAIN";

QueueHandle_t work_queue;
uint32_t TARGET_MASK[2];

void app_main(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,           // Baud rate
        .data_bits = UART_DATA_8_BITS, // Data bits
        .parity = UART_PARITY_DISABLE, // No parity
        .stop_bits = UART_STOP_BITS_1, // 1 stop bit
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "UART configured");

    work_queue = xQueueCreate(10, sizeof(uint8_t *));
    if (work_queue == NULL) {
        printf("Failed to create queue!\n");
        return;
    }
    xTaskCreate(receive_task, "receive_task", 4096, NULL, 10, NULL);
    xTaskCreate(mine_block_task, "mine_block_task", 4096, NULL, 10, NULL);

#if CONFIG_MOCK_MESSAGES

    ESP_LOGI("MAIN", "Sending mock ticket mask");
    uart_set_loop_back(UART_NUM, true);
    uart_write_bytes(UART_NUM, MOCK_TICKET_MASK, 15);

    // Add a delay so mask can actually get set
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t *p_bh = malloc(80);
    memcpy(p_bh, MOCK_BLOCK_HEADER, 80);
    ESP_LOGI("MAIN", "Sending mock block header");
    if (xQueueSend(work_queue, &p_bh, portMAX_DELAY) != pdPASS) {
        printf("Failed to send block header to queue!\n");
    }
#endif
}
