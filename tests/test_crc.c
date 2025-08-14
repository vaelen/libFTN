/*
 * test_crc - CRC Test Suite
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "../include/ftn.h"
#include <assert.h>

static void test_crc_calculation(void) {
    unsigned int crc;
    
    printf("Testing CRC calculation...\n");
    
    /* Test known CRC values */
    crc = ftn_crc16("", 0);
    assert(crc == 0);
    
    crc = ftn_crc16("A", 1);
    assert(crc != 0);
    
    crc = ftn_crc16("123456789", 9);
    /* This should be a known CRC value for this string */
    printf("CRC of '123456789': %u\n", crc);
    
    printf("CRC calculation: PASSED\n");
}

int main(void) {
    printf("Running CRC tests...\n\n");
    
    test_crc_calculation();
    
    printf("\nAll CRC tests passed!\n");
    return 0;
}