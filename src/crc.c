/*
 * libFTN - CRC-16 Implementation
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "../include/ftn.h"

/* CRC-16 polynomial: x^16 + x^12 + x^5 + 1 (0x1021) */
static unsigned int crc_table[256];
static int crc_table_initialized = 0;

static void init_crc_table(void) {
    unsigned int i, j;
    unsigned int crc;
    
    for (i = 0; i < 256; i++) {
        crc = i << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
        crc_table[i] = crc & 0xFFFF;
    }
    crc_table_initialized = 1;
}

unsigned int ftn_crc16(const char* data, size_t length) {
    unsigned int crc = 0;
    size_t i;
    
    if (!crc_table_initialized) {
        init_crc_table();
    }
    
    for (i = 0; i < length; i++) {
        crc = (crc << 8) ^ crc_table[((crc >> 8) ^ (unsigned char)data[i]) & 0xFF];
        crc &= 0xFFFF;
    }
    
    return crc;
}

ftn_error_t ftn_nodelist_verify_crc(const char* filename, unsigned int expected_crc) {
    FILE* fp;
    char* buffer;
    long file_size;
    unsigned int calculated_crc;
    char line[1024];
    
    fp = fopen(filename, "rb");
    if (!fp) {
        return FTN_ERROR_FILE;
    }
    
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        return FTN_ERROR_NOMEM;
    }
    
    /* Skip first line and read the rest for CRC calculation */
    if (fgets(line, sizeof(line), fp) == NULL) {
        free(buffer);
        fclose(fp);
        return FTN_ERROR_FILE;
    }
    
    file_size = 0;
    while (!feof(fp)) {
        int c = fgetc(fp);
        if (c == EOF) break;
        if (c == 0x1A) break; /* EOF marker */
        buffer[file_size++] = c;
    }
    
    fclose(fp);
    
    calculated_crc = ftn_crc16(buffer, file_size);
    free(buffer);
    
    if (calculated_crc != expected_crc) {
        return FTN_ERROR_CRC;
    }
    
    return FTN_OK;
}