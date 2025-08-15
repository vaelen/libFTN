/*
 * test_compat - Compatibility Layer Test Suite
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "../include/ftn.h"
#include <assert.h>

static void test_snprintf(void) {
    char buffer[64];
    int result;
    
    printf("Testing snprintf...\n");
    
    /* Test basic formatting */
    result = snprintf(buffer, sizeof(buffer), "Hello %s", "World");
    assert(result == 11);
    assert(strcmp(buffer, "Hello World") == 0);
    
    /* Test truncation */
    result = snprintf(buffer, 8, "Hello World");
    assert(result == 11); /* Should return full length */
    assert(strcmp(buffer, "Hello W") == 0); /* Should be truncated */
    
    /* Test address formatting */
    result = snprintf(buffer, sizeof(buffer), "%u:%u/%u.%u", 21, 1, 101, 5);
    assert(result > 0);
    assert(strcmp(buffer, "21:1/101.5") == 0);
    
    /* Test edge case - buffer size 0 */
    result = snprintf(NULL, 0, "test");
    assert(result == -1);
    
    /* Test edge case - buffer size 1 */
    result = snprintf(buffer, 1, "test");
    assert(result == 4);
    assert(buffer[0] == '\0'); /* Should be empty string */
    
    printf("snprintf: PASSED\n");
}

static void test_strdup(void) {
    char* result;
    
    printf("Testing strdup...\n");
    
    /* Test basic functionality */
    result = strdup("Hello World");
    assert(result != NULL);
    assert(strcmp(result, "Hello World") == 0);
    free(result);
    
    /* Test empty string */
    result = strdup("");
    assert(result != NULL);
    assert(strcmp(result, "") == 0);
    free(result);
    
    /* Test NULL input */
    result = strdup(NULL);
    assert(result == NULL);
    
    printf("strdup: PASSED\n");
}

static void test_strtok_r(void) {
    char buffer[64];
    char* token;
    char* saveptr;
    
    printf("Testing strtok_r...\n");
    
    /* Test basic tokenization */
    strcpy(buffer, "one,two,three");
    token = strtok_r(buffer, ",", &saveptr);
    assert(token != NULL);
    assert(strcmp(token, "one") == 0);
    
    token = strtok_r(NULL, ",", &saveptr);
    assert(token != NULL);
    assert(strcmp(token, "two") == 0);
    
    token = strtok_r(NULL, ",", &saveptr);
    assert(token != NULL);
    assert(strcmp(token, "three") == 0);
    
    token = strtok_r(NULL, ",", &saveptr);
    assert(token == NULL);
    
    /* Test with multiple delimiters */
    strcpy(buffer, "IBN:test.org,ITN:23");
    token = strtok_r(buffer, ",", &saveptr);
    assert(token != NULL);
    assert(strcmp(token, "IBN:test.org") == 0);
    
    token = strtok_r(NULL, ",", &saveptr);
    assert(token != NULL);
    assert(strcmp(token, "ITN:23") == 0);
    
    token = strtok_r(NULL, ",", &saveptr);
    assert(token == NULL);
    
    /* Test with leading delimiters */
    strcpy(buffer, ",,,one,two");
    token = strtok_r(buffer, ",", &saveptr);
    assert(token != NULL);
    assert(strcmp(token, "one") == 0);
    
    token = strtok_r(NULL, ",", &saveptr);
    assert(token != NULL);
    assert(strcmp(token, "two") == 0);
    
    /* Test with trailing delimiters */
    strcpy(buffer, "one,two,,,");
    token = strtok_r(buffer, ",", &saveptr);
    assert(token != NULL);
    assert(strcmp(token, "one") == 0);
    
    token = strtok_r(NULL, ",", &saveptr);
    assert(token != NULL);
    assert(strcmp(token, "two") == 0);
    
    token = strtok_r(NULL, ",", &saveptr);
    assert(token == NULL);
    
    /* Test empty string */
    strcpy(buffer, "");
    token = strtok_r(buffer, ",", &saveptr);
    assert(token == NULL);
    
    /* Test only delimiters */
    strcpy(buffer, ",,,");
    token = strtok_r(buffer, ",", &saveptr);
    assert(token == NULL);
    
    /* Test NULL inputs */
    token = strtok_r(NULL, ",", NULL);
    assert(token == NULL);
    
    token = strtok_r(buffer, NULL, &saveptr);
    assert(token == NULL);
    
    printf("strtok_r: PASSED\n");
}

int main(void) {
    printf("Running compatibility tests...\n\n");
    
    test_snprintf();
    test_strdup();
    test_strtok_r();
    
    printf("\nAll compatibility tests passed!\n");
    return 0;
}