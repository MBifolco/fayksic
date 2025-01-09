#include "driver/uart.h"
#include "esp_log.h"
#include "main.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 2048       // Buffer size for incoming data
#define UART_NUM UART_NUM_1 // Use UART1
#define TICKET_MASK 0x14

void prettyHex(unsigned char *buf, int len) {
    int i;
    printf("[");
    for (i = 0; i < len - 1; i++) {
        printf("%02X ", buf[i]);
    }
    printf("%02X]", buf[len - 1]);
}

void receive_task() {

    // TODO: Why don't we just make `data` uint8_t?
    unsigned char *data = malloc(BUF_SIZE);
    const char *PARSE_TAG = "PARSE";
    ESP_LOGI(PARSE_TAG, "receive_task up");

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
                    TARGET_MASK[0] |= (uint32_t)(*(data + 6)) << 24;
                    TARGET_MASK[0] |= (uint32_t)(*(data + 7)) << 16;
                    TARGET_MASK[0] |= (uint32_t)(*(data + 8)) << 8;
                    TARGET_MASK[0] |= (uint32_t)(*(data + 9));

                    TARGET_MASK[1] |= (uint32_t)(*(data + 10)) << 24;
                    TARGET_MASK[1] |= (uint32_t)(*(data + 11)) << 16;
                    TARGET_MASK[1] |= (uint32_t)(*(data + 12)) << 8;
                    TARGET_MASK[1] |= (uint32_t)(*(data + 13));
                    ESP_LOGI(PARSE_TAG, "Target mask: 0x%08lx%08lx\n", TARGET_MASK[0], TARGET_MASK[1]);
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