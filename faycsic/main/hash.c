#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "mbedtls/sha256.h"



// Convert a byte array to a hex string for readability (optional)
void to_hex(unsigned char *hash, char output[]) {
    for (int i = 0; i < 32; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
}

// Perform double SHA-256 hashing
void double_sha256(unsigned char *input, size_t len, uint8_t output[32]) {
    unsigned char temp[32];

    mbedtls_sha256(input, strlen((const char *)input), temp,0);
    mbedtls_sha256(temp, strlen((const char *)temp), output,0);
}

void hash(uint8_t block_header[80]) {
    unsigned char hash[32];
    char hex_output[32 * 2 + 1];

    double_sha256(block_header, sizeof(block_header), hash);

        // Output the hash as a hexadecimal string
    to_hex(hash, hex_output);
    printf("Result Hash: %s\n", hex_output);
}