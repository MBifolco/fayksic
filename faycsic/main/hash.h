#ifndef SHA256_H
#define SHA256_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h> // For size_t
#include "esp_system.h"
#include "mbedtls/sha256.h"

// Function declaration
void to_hex(unsigned char *hash, char output[]);
void double_sha256(unsigned char *input, size_t len, uint8_t output[32]);
int hash();
void toHexString(unsigned char *input, char output[], int len);

#endif // SHA256_H