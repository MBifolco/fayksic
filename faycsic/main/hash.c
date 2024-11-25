#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "mbedtls/sha256.h"



// Convert a byte array to a hex string for readability (optional)
void toHexString(unsigned char *input, char output[], int len) {
    for (int i = 0; i < len; i++) {
        sprintf(output + (i * 2), "%02x", input[i]);
    }
}

// Perform double SHA-256 hashing
void double_sha256(unsigned char *input, size_t len, uint8_t output[32]) {
    unsigned char temp[32];

    // First SHA-256 hash
    mbedtls_sha256(input, len, temp, 0);

    // Second SHA-256 hash
    mbedtls_sha256(temp, 32, output, 0);
}
void hash(uint8_t block_header[80]) {
    unsigned char hash[32];
    char hex_output[32 * 2 + 1];

    
    double_sha256(block_header, 80, hash);

        // Output the hash as a hexadecimal string
    toHexString(hash, hex_output, 32);
    printf("Result Hash: %s\n", hex_output);
}