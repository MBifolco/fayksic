#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h> // For size_t
#include "esp_system.h"
#include "mbedtls/sha256.h"

// Function declaration
void double_sha256(unsigned char *input, size_t len, uint8_t output[32]);
void to_hex_string(unsigned char *input, char *output, int len);
int is_valid_hash_32(const uint8_t hash[32], uint32_t target);
uint32_t mine_block(uint8_t block_header[80], uint32_t target_int);
void mine_block_task(void *pvParameters);