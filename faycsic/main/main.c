#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
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
            
            } else {
                ESP_LOGW("PARSE", "Unknown packet type");
                return;
            }

            unsigned char length = data[3];
            ESP_LOGI("PARSE", "length: %02X", (unsigned char)length);

            int data_len = (header & TYPE_JOB) ? (length - 6) : (length - 5); // Exclude CRC size
            ESP_LOGI("PARSE", "data_len: %d", data_len);

            unsigned char *block_header = (uint8_t *)malloc(data_len);
            memcpy(block_header, data + 4, data_len);

            prettyHex((unsigned char *)block_header, data_len);
            printf("\n");
            
            //char block_header_str[160];
            //toHexString(block_header, block_header_str, 80);
            //printf("Result block header: %s\n", block_header_str);


            //hash(block_header);
           
        }

       
    }
}

void app_main(void)
{
    ESP_LOGI("main", "Hello, world!");
    xTaskCreate(echo_task, "uart_echo_task", 2048, NULL, 10, NULL);
}