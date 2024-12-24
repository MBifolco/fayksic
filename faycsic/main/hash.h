#include "esp_system.h"
#include "mbedtls/sha256.h"
#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Function declaration
void double_sha256(unsigned char *input, size_t len, uint8_t output[32]);
void to_hex_string(uint8_t *input, char *output, int len);
int is_valid_hash_32(const uint8_t hash[32], uint32_t target);
uint32_t mine_block(uint8_t block_header[80], uint32_t target_int);
void mine_block_task(void *pvParameters);