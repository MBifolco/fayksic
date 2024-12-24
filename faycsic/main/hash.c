#include "esp_log.h"
#include "globals.h"
#include "mbedtls/sha256.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "HASH";

// Convert a byte array to a hex string for readability (optional)
void to_hex_string(uint8_t *input, char *output, int len) {
    for (int i = 0; i < len; i++) {
        sprintf(output + (i * 2), "%02x", input[i]);
    }
    output[len * 2] = '\0'; // Null-terminate the output string
}

// Perform double SHA-256 hashing
void double_sha256(unsigned char *input, size_t len, uint8_t output[32]) {
    unsigned char temp[32];

    // First SHA-256 hash
    mbedtls_sha256(input, len, temp, 0);

    // Second SHA-256 hash
    mbedtls_sha256(temp, 32, output, 0);
}

// Compare the first 4 bytes of the hash (big-endian) to a 32-bit target.
// Return 1 if hash <= target, else 0.
int is_valid_hash_32(const uint8_t hash[32]) {
    // Flip the hash bytes around to conform to what our target mask lloks like
    // TODO: Verify with a real ASIC this is indeed the correct ordering
    uint32_t high = ((uint32_t)hash[31] << 24) | ((uint32_t)hash[30] << 16) | ((uint32_t)hash[29] << 8) | ((uint32_t)hash[28]);
    uint32_t low = ((uint32_t)hash[27] << 24) | ((uint32_t)hash[26] << 16) | ((uint32_t)hash[25] << 8) | ((uint32_t)hash[24]);

    // ESP_LOGI(TAG, "Hash:\t0x%08lx%08lx", high, low);
    // ESP_LOGI(TAG, "Mask:\t0x%08lx%08lx", target_difficulty[0], target_difficulty[1]);
    // ESP_LOGI(TAG, "high <=? m[0]: %d", high < target_difficulty[0]);
    // ESP_LOGI(TAG, "low <=? m[1]: %d", low < target_difficulty[1]);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    if (high > target_difficulty[0]) {
        return 0;
    }
    if (high == target_difficulty[0] && low > target_difficulty[1]) {
        return 0;
    }
    return 1;
}

// Mine the block with a 32-bit difficulty target
uint32_t mine_block(uint8_t block_header[80]) {
    uint8_t hash[32];
    uint32_t nonce;

    ESP_LOGI(TAG, "Received block header");
    for (int i = 0; i < 80; i++)
        printf("%02x", block_header[i]);
    printf("\n");

    ESP_LOGI(TAG, "Starting mining");
    // Start here for block 100,000 to quickly produced the block 100,000 hash
    // for (nonce = 0x10572B00; nonce <= 0xFFFFFFFF; nonce++) {
    for (nonce = 0; nonce <= 0xFFFFFFFF; nonce++) {
        if (nonce % 10000 == 0 && nonce > 0) {
            vTaskDelay(pdMS_TO_TICKS(10)); // Yield time to other tasks
        }

        if (nonce % 500000 == 0 && nonce > 0) {
            ESP_LOGI(TAG, "At nonce %ld (0x%08lx)", nonce, nonce);
        }

        // Update nonce in the block header (last 4 bytes)
        // 0x 0f 2b 57 10
        block_header[76] = (nonce & 0x000000FF);
        block_header[77] = (nonce & 0x0000FF00) >> 8;
        block_header[78] = (nonce & 0x00FF0000) >> 16;
        block_header[79] = (nonce & 0xFF000000) >> 24;

        // Compute the double SHA-256 hash
        double_sha256(block_header, 80, hash);

        // Check if the hash meets the target
        if (is_valid_hash_32(hash)) {
            char h[65];
            to_hex_string(hash, h, 32);
            ESP_LOGI(TAG, "Valid nonce found: 0x%08lx", nonce);
            ESP_LOGI(TAG, "Produced: %s", h);
            // return nonce;
        }
    }

    printf("Nonce space exhausted\n");
    return 0;
}

void mine_block_task(void *pvParameters) {
    uint8_t *block_header;

    // Example: Convert '00 00 80 FF' to big-endian target.
    // If given in little-endian, '00 00 80 FF' = 'FF800000' big-endian.

    ESP_LOGI(TAG, "Task up - waiting for headers");
    while (true) {
        if (xQueueReceive(work_queue, &block_header, portMAX_DELAY) == pdPASS) {
            // Mine the block with the 32-bit target
            uint32_t nonce = mine_block(block_header);
            free(block_header);
            if (nonce == 0) {
                ESP_LOGE(TAG, "No valid nonce found.");
            }
        } else {
            ESP_LOGW(TAG, "Failed to receive job from the queue");
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Yield to let other tasks run
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}