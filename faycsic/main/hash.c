#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "mbedtls/sha256.h"

#include "esp_log.h"

static const char *TAG = "HASH";

// Convert a byte array to a hex string for readability (optional)
void to_hex_string(unsigned char *input, char *output, int len) {
    for (int i = 0; i < len; i++) {
        sprintf(output + (i * 2), "%02x", input[i]);
    }
    output[len * 2] = '\0';  // Null-terminate the output string
}

// Perform double SHA-256 hashing
void double_sha256(unsigned char *input, size_t len, uint8_t output[32]) {
    unsigned char temp[32];

    // First SHA-256 hash
    mbedtls_sha256(input, len, temp, 0);

    // Second SHA-256 hash
    mbedtls_sha256(temp, 32, output, 0);
}

void make_hash(uint8_t block_header[80]) {
    unsigned char *block_hash = malloc(32);
    if (!block_hash) {
        ESP_LOGE(TAG, "Failed to allocate memory for block_hash");
        return;
    }

    double_sha256(block_header, 80, block_hash);

    char *hex_output = malloc(32 * 2);  // +1 for null terminator
    if (!hex_output) {
        ESP_LOGE(TAG, "Failed to allocate memory for hex_output");
        free(block_hash);
        return;
    }

    to_hex_string(block_hash, hex_output, 32);
    printf("Result Hash: %s\n", hex_output);

    free(block_hash);
    free(hex_output);
}

// Helper function to compare a hash with the target
int is_valid_hash(const uint8_t hash[32], const uint8_t target[32]) {
    for (int i = 0; i < 32; i++) {
        if (hash[i] < target[i]) return 1;  // Hash is smaller than target
        if (hash[i] > target[i]) return 0;  // Hash is larger than target
    }
    return 1;  // Hash is equal to target
}

uint32_t mine_block(uint8_t block_header[80], const uint8_t target[32]) {
    uint8_t hash[32];
    uint32_t nonce = 0;

    printf("Starting mining...\n");
    for (nonce = 0; nonce <= 0xFFFFFFFF; nonce++) {
        // Update nonce in the block header (last 4 bytes)
        block_header[76] = (nonce & 0x000000FF);
        block_header[77] = (nonce & 0x0000FF00) >> 8;
        block_header[78] = (nonce & 0x00FF0000) >> 16;
        block_header[79] = (nonce & 0xFF000000) >> 24;

        // Compute the double SHA-256 hash
        double_sha256(block_header, 80, hash);

        // Check if the hash meets the target
        if (is_valid_hash(hash, target)) {
            printf("Valid nonce found: %lu\n", nonce);
            printf("Hash: ");
            for (int i = 0; i < 32; i++) printf("%02x", hash[i]);
            printf("\n");
            return nonce;
        }
    }

    printf("Nonce space exhausted, no valid hash found.\n");
    return 0;
}


