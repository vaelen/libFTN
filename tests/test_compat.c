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

static void test_strcasecmp(void) {
    printf("Testing strcasecmp...\n");
    
    /* Test equal strings */
    assert(strcasecmp("hello", "hello") == 0);
    
    /* Test case-insensitive equal strings */
    assert(strcasecmp("Hello", "hello") == 0);
    assert(strcasecmp("HELLO", "hello") == 0);
    assert(strcasecmp("hello", "HELLO") == 0);
    
    /* Test different strings */
    assert(strcasecmp("hello", "world") != 0);
    
    /* Test ordering */
    assert(strcasecmp("apple", "BANANA") < 0);
    assert(strcasecmp("ZEBRA", "apple") > 0);
    
    /* Test null handling */
    assert(strcasecmp(NULL, NULL) == 0);
    assert(strcasecmp("hello", NULL) > 0);
    assert(strcasecmp(NULL, "hello") < 0);
    
    /* Test mixed case */
    assert(strcasecmp("MiXeD", "mixed") == 0);
    assert(strcasecmp("Test123", "TEST123") == 0);
    
    printf("strcasecmp: PASSED\n");
}

static void test_strncasecmp(void) {
    printf("Testing strncasecmp...\n");
    
    /* Test equal strings within limit */
    assert(strncasecmp("hello", "hello", 5) == 0);
    
    /* Test case-insensitive equal strings */
    assert(strncasecmp("Hello", "hello", 5) == 0);
    assert(strncasecmp("HELLO", "hello", 5) == 0);
    
    /* Test partial comparison */
    assert(strncasecmp("hello", "help", 3) == 0);
    assert(strncasecmp("hello", "help", 4) != 0);
    
    /* Test with zero length */
    assert(strncasecmp("hello", "world", 0) == 0);
    
    /* Test ordering */
    assert(strncasecmp("apple", "BANANA", 1) < 0);
    assert(strncasecmp("ZEBRA", "apple", 1) > 0);
    
    /* Test null handling */
    assert(strncasecmp(NULL, NULL, 5) == 0);
    assert(strncasecmp("hello", NULL, 5) > 0);
    assert(strncasecmp(NULL, "hello", 5) < 0);
    
    /* Test length limits */
    assert(strncasecmp("testing", "TEST", 4) == 0);
    assert(strncasecmp("testing", "TEST", 5) != 0);
    
    printf("strncasecmp: PASSED\n");
}

static void test_strlcpy(void) {
    char buffer[32];
    size_t result;
    
    printf("Testing strlcpy...\n");
    
    /* Test basic functionality */
    result = strlcpy(buffer, "Hello World", sizeof(buffer));
    assert(result == 11);
    assert(strcmp(buffer, "Hello World") == 0);
    
    /* Test truncation */
    result = strlcpy(buffer, "This is a very long string that will be truncated", 10);
    assert(result == 49); /* Should return full source length */
    assert(strlen(buffer) == 9); /* Should be truncated to fit */
    assert(strcmp(buffer, "This is a") == 0);
    assert(buffer[9] == '\0'); /* Should be null-terminated */
    
    /* Test exact fit */
    result = strlcpy(buffer, "1234567890", 11);
    assert(result == 10);
    assert(strcmp(buffer, "1234567890") == 0);
    
    /* Test buffer size 1 */
    result = strlcpy(buffer, "test", 1);
    assert(result == 4);
    assert(strcmp(buffer, "") == 0);
    assert(buffer[0] == '\0');
    
    /* Test buffer size 0 */
    result = strlcpy(NULL, "test", 0);
    assert(result == 4);
    
    /* Test NULL source */
    result = strlcpy(buffer, NULL, sizeof(buffer));
    assert(result == 0);
    assert(buffer[0] == '\0');
    
    /* Test NULL source with NULL destination */
    result = strlcpy(NULL, NULL, 0);
    assert(result == 0);
    
    /* Test empty string */
    result = strlcpy(buffer, "", sizeof(buffer));
    assert(result == 0);
    assert(strcmp(buffer, "") == 0);
    
    /* Test FTN address formatting scenario */
    result = strlcpy(buffer, "21:1/101.5", sizeof(buffer));
    assert(result == 10);
    assert(strcmp(buffer, "21:1/101.5") == 0);
    
    /* Test username truncation scenario */
    result = strlcpy(buffer, "very_long_username_that_exceeds_buffer", 16);
    assert(result == 38);
    assert(strlen(buffer) == 15);
    assert(strcmp(buffer, "very_long_usern") == 0);
    assert(buffer[15] == '\0');
    
    /* Test one character source */
    result = strlcpy(buffer, "A", sizeof(buffer));
    assert(result == 1);
    assert(strcmp(buffer, "A") == 0);
    
    /* Test buffer exactly one larger than source */
    result = strlcpy(buffer, "test", 5);
    assert(result == 4);
    assert(strcmp(buffer, "test") == 0);
    
    printf("strlcpy: PASSED\n");
}

int main(void) {
    printf("Running compatibility tests...\n\n");
    
    test_snprintf();
    test_strdup();
    test_strtok_r();
    test_strcasecmp();
    test_strncasecmp();
    test_strlcpy();
    
    printf("\nAll compatibility tests passed!\n");
    return 0;
}