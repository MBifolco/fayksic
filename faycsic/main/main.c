#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "globals.h"
#include <freertos/queue.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/uart.h"

#include "sdkconfig.h"

#include "hash.h"

#if CONFIG_MOCK_MESSAGES
#include "mock_hash.h"
#endif

#define UART_NUM UART_NUM_1 // Use UART1
#define BUF_SIZE 2048       // Buffer size for incoming data
#define RX_PIN 12           // GPIO pin for UART RX
#define TX_PIN 13           // GPIO pin for UART TX
#define PREAMBLE_1 0x55
#define PREAMBLE_2 0xAA
#define EVENT_QUEUE_SIZE 10
#define TYPE_JOB 0x20
#define TYPE_CMD 0x40

#define GROUP_SINGLE 0x00
#define GROUP_ALL 0x10

#define CMD_JOB 0x01

#define CMD_SETADDRESS 0x00
#define CMD_WRITE 0x01
#define CMD_READ 0x02
#define CMD_INACTIVE 0x03

#define RESPONSE_CMD 0x00
#define RESPONSE_JOB 0x80

#define SLEEP_TIME 20
#define FREQ_MULT 25.0

#define CLOCK_ORDER_CONTROL_0 0x80
#define CLOCK_ORDER_CONTROL_1 0x84
#define ORDERED_CLOCK_ENABLE 0x20
#define CORE_REGISTER_CONTROL 0x3C
#define PLL3_PARAMETER 0x68
#define FAST_UART_CONFIGURATION 0x28
#define TICKET_MASK 0x14
#define MISC_CONTROL 0x18
#define LENGTH 0

static const char *TAG = "MAIN";

void prettyHex(unsigned char *buf, int len) {
    int i;
    printf("[");
    for (i = 0; i < len - 1; i++) {
        printf("%02X ", buf[i]);
    }
    printf("%02X]", buf[len - 1]);
}
// Function to reverse the bits of an 8-bit unsigned char
unsigned char _reverse_bits(unsigned char byte) {
    byte = ((byte & 0xF0) >> 4) | ((byte & 0x0F) << 4); // Swap nibbles
    byte = ((byte & 0xCC) >> 2) | ((byte & 0x33) << 2); // Swap pairs of bits
    byte = ((byte & 0xAA) >> 1) | ((byte & 0x55) << 1); // Swap individual bits
    return byte;
}

uint32_t decode_difficulty(const unsigned char *job_difficulty_mask) {
    uint32_t decoded_difficulty = 0;

    // job_difficulty_mask[2] contains the reversed bits of the MSB
    // job_difficulty_mask[3] contains the reversed bits of the next byte
    // job_difficulty_mask[4] contains the reversed bits of the next byte
    // job_difficulty_mask[5] contains the reversed bits of the LSB

    for (int i = 0; i < 4; i++) {
        // Reverse bits again to get original byte
        unsigned char reversed_val = _reverse_bits(job_difficulty_mask[2 + i]);

        // Now place it back into the integer in the correct order:
        // i=0 -> MSB, i=3 -> LSB
        decoded_difficulty |= ((uint32_t)reversed_val << (8 * (3 - i)));
    }

    return decoded_difficulty;
}

QueueHandle_t work_queue;
uint32_t target_difficulty[2]; // TODO: rename - TARGET_MASK & set default value...

static void receive_task(void *arg) {

    // TODO: Why don't we just make `data` uint8_t?
    unsigned char *data = malloc(BUF_SIZE);
    const char *PARSE_TAG = "PARSE";

    while (1) {
        // Read data from the UART
        int len = uart_read_bytes(UART_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);

        if (len) {
            ESP_LOGI(PARSE_TAG, "Received %d bytes\n", len);
            ESP_LOGI(PARSE_TAG, "Data: ");
            prettyHex((unsigned char *)data, len);
            printf("\n");

            if (len < 4) {
                ESP_LOGW(PARSE_TAG, "Packet too short");
                continue;
            }

            // Validate preamble
            if (data[0] != 0x55 || data[1] != 0xAA) {
                ESP_LOGW(PARSE_TAG, "Invalid preamble");
                continue;
            } else {
                ESP_LOGI(PARSE_TAG, "Preamble OK");
            }

            unsigned char header = data[2];
            ESP_LOGI(PARSE_TAG, "Header: %02X", (unsigned char)header);

            if (header == 0x21) {
                ESP_LOGI(PARSE_TAG, "Job packet");
                unsigned char length = data[3];
                ESP_LOGI(PARSE_TAG, "   length: %02X", (unsigned char)length);

                int data_len = length - 6;
                ESP_LOGI(PARSE_TAG, "   data_len: %d", data_len);

                int work_id = data[4];
                ESP_LOGI(PARSE_TAG, "   work_id: %02X", (unsigned char)work_id);

                int num_midstates = data[5];
                ESP_LOGI(PARSE_TAG, "   num_midstates: %02X", (unsigned char)num_midstates);

                uint8_t *block_header = malloc(data_len);
                memcpy(block_header, data + 6, data_len);

                ESP_LOGI(PARSE_TAG, "   block header: ");
                ESP_LOGI(PARSE_TAG, "Sending block header");
                for (int i = 0; i < 80; i++)
                    printf("%02x", block_header[i]);
                printf("\n");

                if (xQueueSend(work_queue, &block_header, portMAX_DELAY) != pdPASS) {
                    printf("Failed to send block header to queue!\n");
                }
            } else if (header == 0x51) {
                ESP_LOGI(PARSE_TAG, "Command packet");

                // Extract length and data
                uint8_t length = (uint8_t)data[3];
                ESP_LOGI(PARSE_TAG, "   total length: %d", length);

                int data_len = length - 5; // Account for CRC5
                ESP_LOGI(PARSE_TAG, "   data_len: %d", data_len);

                // Handle CRC5 (last byte)
                uint8_t received_crc5 = data[length + 1]; // Length does not include preamble bytes
                ESP_LOGI(PARSE_TAG, "   received_crc5: %02X", received_crc5);

                // Extract the message mask (2 bytes preamble + 2 bytes header + 2 bytes has mask)
                unsigned char mask = *(data + 5);
                ESP_LOGI(PARSE_TAG, "   MASK: %02X", mask);

                if (mask == TICKET_MASK) {
                    target_difficulty[0] |= (uint32_t)(*(data + 6)) << 24;
                    target_difficulty[0] |= (uint32_t)(*(data + 7)) << 16;
                    target_difficulty[0] |= (uint32_t)(*(data + 8)) << 8;
                    target_difficulty[0] |= (uint32_t)(*(data + 9));

                    target_difficulty[1] |= (uint32_t)(*(data + 10)) << 24;
                    target_difficulty[1] |= (uint32_t)(*(data + 11)) << 16;
                    target_difficulty[1] |= (uint32_t)(*(data + 12)) << 8;
                    target_difficulty[1] |= (uint32_t)(*(data + 13));
                    ESP_LOGI(PARSE_TAG, "Target mask: 0x%08lx%08lx\n", target_difficulty[0], target_difficulty[1]);
                } else {
                    ESP_LOGW(PARSE_TAG, "Unknown mask");
                    continue;
                }

                // Use the decoded difficulty value
                // You can process the difficulty or take further actions here
            } else {
                ESP_LOGW(PARSE_TAG, "Unknown packet type");
                continue;
            }
        }
    }
}

void app_main(void) {
    setvbuf(stdout, NULL, _IONBF, 0); // Disable buffering for stdout

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
    ESP_LOGI("MAIN", "UART configured");

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

    ESP_LOGI("MAIN", "Sending mock block header");
    // for (int i = 0; i < 80; i++)
    //     printf("%02x", MOCK_BLOCK_HEADER[i]);
    // printf("\n");

    uint8_t *p_bh = malloc(80);
    memcpy(p_bh, MOCK_BLOCK_HEADER, 80);
    if (xQueueSend(work_queue, &p_bh, portMAX_DELAY) != pdPASS) {
        printf("Failed to send block header to queue!\n");
    }
#endif
}
