#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/queue.h>

#include "driver/uart.h"
#include "driver/gpio.h"

#include "sdkconfig.h"

#include "hash.h"



#define UART_NUM UART_NUM_1       // Use UART1
#define BUF_SIZE 2048             // Buffer size for incoming data
#define RX_PIN 12                 // GPIO pin for UART RX
#define TX_PIN 13                 // GPIO pin for UART TX 
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

void prettyHex(unsigned char *buf, int len)
{
    int i;
    printf("[");
    for (i = 0; i < len - 1; i++)
    {
        printf("%02X ", buf[i]);
    }
    printf("%02X]", buf[len - 1]);
}
// Function to reverse the bits of an 8-bit unsigned char
unsigned char _reverse_bits(unsigned char byte) {
    byte = ((byte & 0xF0) >> 4) | ((byte & 0x0F) << 4);    // Swap nibbles
    byte = ((byte & 0xCC) >> 2) | ((byte & 0x33) << 2);    // Swap pairs of bits
    byte = ((byte & 0xAA) >> 1) | ((byte & 0x55) << 1);    // Swap individual bits
    return byte;
}

static const char *TAG = "MAIN";

static void echo_task(void *arg) {
   

    const uart_config_t uart_config = {
        .baud_rate = 115200,              // Baud rate
        .data_bits = UART_DATA_8_BITS,    // Data bits
        .parity = UART_PARITY_DISABLE,    // No parity
        .stop_bits = UART_STOP_BITS_1,    // 1 stop bit
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI("main", "UART configured");

    char *data = (char *) malloc(BUF_SIZE);

    while (1) {
        // Read data from the UART
        int len = uart_read_bytes(UART_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);

        if (len) {
            printf("Received %d bytes\n", len);
            printf("message: ");
            prettyHex((unsigned char *)data, len);

            
            printf("\n");

            if (len < 4) {
                ESP_LOGW("PARSE", "Packet too short");
                return;
            }

            // Validate preamble
            if (data[0] != 0x55 || data[1] != 0xAA) {
                ESP_LOGW("PARSE", "Invalid preamble");
                return;
            } else {
                ESP_LOGI("PARSE", "Preamble OK");
            }

            unsigned char header = data[2];
            ESP_LOGI("PARSE", "header: %02X", (unsigned char)header);

            if (header == 0x21) {
                ESP_LOGI("PARSE", "Job packet");
                unsigned char length = data[3];
                ESP_LOGI("PARSE", "length: %02X", (unsigned char)length);

                int data_len = length - 6;
                ESP_LOGI("PARSE", "data_len: %d", data_len);

                int work_id = data[4];
                ESP_LOGI("PARSE", "work_id: %02X", (unsigned char)work_id);

                int num_midstates = data[5];
                ESP_LOGI("PARSE", "num_midstates: %02X", (unsigned char)num_midstates);

                unsigned char *block_header = (uint8_t *)malloc(data_len);
                memcpy(block_header, data + 6, data_len);

                printf("block header: ");
                prettyHex((unsigned char *)block_header, 80);

                make_hash(block_header);
            }else if (header == 0x51) {
                ESP_LOGI("PARSE", "Command packet");

                // Extract length and data
                unsigned char length = data[3];
                ESP_LOGI("PARSE", "length: %02X", (unsigned char)length);

                int data_len = length - 5;  // Account for CRC5
                ESP_LOGI("PARSE", "data_len: %d", data_len);

                // Handle CRC5 (last byte)
                uint8_t received_crc5 = data[4 + data_len];
                ESP_LOGI("PARSE", "Received CRC5: %02X", received_crc5);

                // Extract the message mask (last 6 bytes before CRC5)
                unsigned char *message = (unsigned char *)(data + length - 5);
                ESP_LOGI("Mask: ");
                prettyHex(message, 6);

                // Extract the padding byte and TICKET_MASK (last two bytes after the difficulty mask)
                unsigned char padding_byte = message[0];
                unsigned char mask = message[1];
                ESP_LOGI("PARSE", "Padding byte: %02X", padding_byte);
                ESP_LOGI("PARSE", "MASK: %02X", mask);

                if (mask == TICKET_MASK) {
                    ESP_LOGI("PARSE", "this is a set difficulty command");
                    // Decode the difficulty value from the mask
                    uint32_t decoded_difficulty = 0;
                    for (int i = 0; i < 4; i++) {
                        unsigned char value = message[5 - i];  // Reverse byte order
                        decoded_difficulty |= (_reverse_bits(value) << (8 * i));
                    }

                    // Log the decoded difficulty
                    ESP_LOGI("PARSE", "Decoded difficulty: %lu", decoded_difficulty);
                } else {
                    ESP_LOGW("PARSE", "Unknown mask");
                    return;
                }

                

            // Use the decoded difficulty value
            // You can process the difficulty or take further actions here
            } else {
                ESP_LOGW("PARSE", "Unknown packet type");
                return;
            }

            
        }
    }
}

void app_main(void)
{
    ESP_LOGI("main", "Hello, world!");
    xTaskCreate(echo_task, "uart_echo_task", 2048, NULL, 10, NULL);
}



void parse_job_difficulty_mask(const unsigned char *payload) {
    // Assuming payload starts after the preamble and command header
    const unsigned char *job_difficulty_mask = payload;

    // Verify the mask length
    if (job_difficulty_mask == NULL) {
        ESP_LOGE(TAG, "Invalid payload: NULL job difficulty mask");
        return;
    }

    // Extract and reconstruct the difficulty value
    uint32_t difficulty = 0;
    for (int i = 0; i < 4; i++) {
        unsigned char reversed_byte = _reverse_bits(job_difficulty_mask[5 - i]);
        difficulty |= (reversed_byte << (8 * i));
    }

    // Log the parsed difficulty
    ESP_LOGI(TAG, "Parsed job difficulty mask: %lu", difficulty);

    // Additional validation or processing if needed
}