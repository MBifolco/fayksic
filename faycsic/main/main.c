#include <stdio.h>
#include "esp_log.h"
#include "hash.h"
void app_main(void)
{
    ESP_LOGI("main", "Hello, world!");

    // Example: Simplified Bitcoin block header
    uint8_t block_header[80] = { /* 80 bytes of block header data */ };

    // Check if hash meets the difficulty target
    unsigned char target[32] = { /* Difficulty target */ };
    target[0] = 0xf;

    // run hash
    hash(target, block_header);
}