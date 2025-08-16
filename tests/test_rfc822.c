/*
 * libFTN - RFC822 Message Format Tests
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "../include/ftn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int test_count = 0;
static int test_passed = 0;

#define TEST(name) do { \
    printf("Testing %s... ", #name); \
    test_count++; \
    if ((test_##name())) { \
        printf("PASS\n"); \
        test_passed++; \
    } else { \
        printf("FAIL\n"); \
    } \
} while(0)

/* Test RFC822 message creation and basic operations */
static int test_message_creation(void) {
    rfc822_message_t* msg;
    const char* from;
    const char* subject;
    const char* to_lower;
    
    msg = rfc822_message_new();
    if (!msg) return 0;
    
    /* Test adding headers */
    if (rfc822_message_add_header(msg, "From", "test@example.com") != FTN_OK) {
        rfc822_message_free(msg);
        return 0;
    }
    
    if (rfc822_message_add_header(msg, "To", "recipient@example.com") != FTN_OK) {
        rfc822_message_free(msg);
        return 0;
    }
    
    if (rfc822_message_add_header(msg, "Subject", "Test Message") != FTN_OK) {
        rfc822_message_free(msg);
        return 0;
    }
    
    /* Test header retrieval */
    from = rfc822_message_get_header(msg, "From");
    if (!from || strcmp(from, "test@example.com") != 0) {
        rfc822_message_free(msg);
        return 0;
    }
    
    subject = rfc822_message_get_header(msg, "Subject");
    if (!subject || strcmp(subject, "Test Message") != 0) {
        rfc822_message_free(msg);
        return 0;
    }
    
    /* Test case-insensitive header lookup */
    to_lower = rfc822_message_get_header(msg, "to");
    if (!to_lower || strcmp(to_lower, "recipient@example.com") != 0) {
        rfc822_message_free(msg);
        return 0;
    }
    
    /* Test setting body */
    if (rfc822_message_set_body(msg, "This is a test message body.") != FTN_OK) {
        rfc822_message_free(msg);
        return 0;
    }
    
    rfc822_message_free(msg);
    return 1;
}

/* Test RFC822 message parsing */
static int test_message_parsing(void) {
    const char* rfc822_text = 
        "From: sender@example.com\r\n"
        "To: recipient@example.com\r\n"
        "Subject: Test Message\r\n"
        "Date: Mon, 01 Jan 2024 12:00:00 GMT\r\n"
        "\r\n"
        "This is the message body.\r\n"
        "It has multiple lines.\r\n";
    
    rfc822_message_t* msg = NULL;
    const char* from;
    const char* subject;
    
    if (rfc822_message_parse(rfc822_text, &msg) != FTN_OK) {
        return 0;
    }
    
    if (!msg) return 0;
    
    /* Check headers */
    from = rfc822_message_get_header(msg, "From");
    if (!from || strcmp(from, "sender@example.com") != 0) {
        rfc822_message_free(msg);
        return 0;
    }
    
    subject = rfc822_message_get_header(msg, "Subject");
    if (!subject || strcmp(subject, "Test Message") != 0) {
        rfc822_message_free(msg);
        return 0;
    }
    
    /* Check body */
    if (!msg->body || strstr(msg->body, "This is the message body.") == NULL) {
        rfc822_message_free(msg);
        return 0;
    }
    
    rfc822_message_free(msg);
    return 1;
}

/* Test RFC822 message generation */
static int test_message_generation(void) {
    rfc822_message_t* msg;
    char* text;
    
    msg = rfc822_message_new();
    if (!msg) return 0;
    
    /* Add headers */
    rfc822_message_add_header(msg, "From", "sender@example.com");
    rfc822_message_add_header(msg, "To", "recipient@example.com");
    rfc822_message_add_header(msg, "Subject", "Test Message");
    rfc822_message_set_body(msg, "This is a test message.");
    
    /* Generate text */
    text = rfc822_message_to_text(msg);
    if (!text) {
        rfc822_message_free(msg);
        return 0;
    }
    
    /* Check that generated text contains expected elements */
    if (strstr(text, "From: sender@example.com") == NULL ||
        strstr(text, "To: recipient@example.com") == NULL ||
        strstr(text, "Subject: Test Message") == NULL ||
        strstr(text, "This is a test message.") == NULL) {
        free(text);
        rfc822_message_free(msg);
        return 0;
    }
    
    /* Check that headers and body are separated by empty line */
    if (strstr(text, "\r\n\r\n") == NULL) {
        free(text);
        rfc822_message_free(msg);
        return 0;
    }
    
    free(text);
    rfc822_message_free(msg);
    return 1;
}

/* Test FTN address to RFC822 conversion */
static int test_ftn_address_to_rfc822(void) {
    ftn_address_t addr = {1, 2, 3, 4};
    char* rfc_addr;
    
    /* Test with name */
    rfc_addr = ftn_address_to_rfc822(&addr, "John Doe", "fidonet.org");
    if (!rfc_addr || strstr(rfc_addr, "John Doe") == NULL || strstr(rfc_addr, "1:2/3.4@fidonet.org") == NULL) {
        free(rfc_addr);
        return 0;
    }
    free(rfc_addr);
    
    /* Test without name */
    rfc_addr = ftn_address_to_rfc822(&addr, NULL, "fidonet.org");
    if (!rfc_addr || strcmp(rfc_addr, "1:2/3.4@fidonet.org") != 0) {
        free(rfc_addr);
        return 0;
    }
    free(rfc_addr);
    
    return 1;
}

/* Test RFC822 address to FTN conversion */
static int test_rfc822_address_to_ftn(void) {
    ftn_address_t addr;
    char* name = NULL;
    
    /* Test with name and brackets */
    if (rfc822_address_to_ftn("\"John Doe\" <1:2/3.4@fidonet.org>", "fidonet.org", &addr, &name) != FTN_OK) {
        return 0;
    }
    
    if (addr.zone != 1 || addr.net != 2 || addr.node != 3 || addr.point != 4) {
        free(name);
        return 0;
    }
    
    if (!name || strcmp(name, "John Doe") != 0) {
        free(name);
        return 0;
    }
    free(name);
    
    /* Test without name */
    name = NULL;
    if (rfc822_address_to_ftn("1:2/3.0@fidonet.org", "fidonet.org", &addr, &name) != FTN_OK) {
        return 0;
    }
    
    if (addr.zone != 1 || addr.net != 2 || addr.node != 3 || addr.point != 0) {
        free(name);
        return 0;
    }
    
    if (name != NULL) {
        free(name);
        return 0;
    }
    
    return 1;
}

/* Test FTN to RFC822 conversion */
static int test_ftn_to_rfc822_conversion(void) {
    ftn_message_t* ftn_msg;
    rfc822_message_t* rfc_msg = NULL;
    const char* from;
    const char* to;
    const char* subject;
    
    ftn_msg = ftn_message_new(FTN_MSG_NETMAIL);
    if (!ftn_msg) return 0;
    
    /* Set up FTN message */
    ftn_msg->orig_addr.zone = 1;
    ftn_msg->orig_addr.net = 2;
    ftn_msg->orig_addr.node = 3;
    ftn_msg->orig_addr.point = 0;
    
    ftn_msg->dest_addr.zone = 1;
    ftn_msg->dest_addr.net = 2;
    ftn_msg->dest_addr.node = 4;
    ftn_msg->dest_addr.point = 0;
    
    ftn_msg->from_user = malloc(strlen("John Doe") + 1);
    strcpy(ftn_msg->from_user, "John Doe");
    
    ftn_msg->to_user = malloc(strlen("Jane Smith") + 1);
    strcpy(ftn_msg->to_user, "Jane Smith");
    
    ftn_msg->subject = malloc(strlen("Test Subject") + 1);
    strcpy(ftn_msg->subject, "Test Subject");
    
    ftn_msg->text = malloc(strlen("Test message body") + 1);
    strcpy(ftn_msg->text, "Test message body");
    
    ftn_msg->timestamp = 1704067200; /* Mon, 01 Jan 2024 00:00:00 GMT */
    
    /* Convert to RFC822 */
    if (ftn_to_rfc822(ftn_msg, "fidonet.org", &rfc_msg) != FTN_OK) {
        ftn_message_free(ftn_msg);
        return 0;
    }
    
    if (!rfc_msg) {
        ftn_message_free(ftn_msg);
        return 0;
    }
    
    /* Check headers */
    from = rfc822_message_get_header(rfc_msg, "From");
    if (!from || strstr(from, "John Doe") == NULL || strstr(from, "1:2/3@fidonet.org") == NULL) {
        rfc822_message_free(rfc_msg);
        ftn_message_free(ftn_msg);
        return 0;
    }
    
    to = rfc822_message_get_header(rfc_msg, "To");
    if (!to || strstr(to, "Jane Smith") == NULL || strstr(to, "1:2/4@fidonet.org") == NULL) {
        rfc822_message_free(rfc_msg);
        ftn_message_free(ftn_msg);
        return 0;
    }
    
    subject = rfc822_message_get_header(rfc_msg, "Subject");
    if (!subject || strcmp(subject, "Test Subject") != 0) {
        rfc822_message_free(rfc_msg);
        ftn_message_free(ftn_msg);
        return 0;
    }
    
    /* Check body */
    if (!rfc_msg->body || strcmp(rfc_msg->body, "Test message body") != 0) {
        rfc822_message_free(rfc_msg);
        ftn_message_free(ftn_msg);
        return 0;
    }
    
    rfc822_message_free(rfc_msg);
    ftn_message_free(ftn_msg);
    return 1;
}

/* Test RFC822 to FTN conversion */
static int test_rfc822_to_ftn_conversion(void) {
    rfc822_message_t* rfc_msg;
    ftn_message_t* ftn_msg = NULL;
    
    rfc_msg = rfc822_message_new();
    if (!rfc_msg) return 0;
    
    /* Set up RFC822 message */
    rfc822_message_add_header(rfc_msg, "From", "\"John Doe\" <1:2/3.0@fidonet.org>");
    rfc822_message_add_header(rfc_msg, "To", "\"Jane Smith\" <1:2/4.0@fidonet.org>");
    rfc822_message_add_header(rfc_msg, "Subject", "Test Subject");
    rfc822_message_add_header(rfc_msg, "Date", "01 Jan 2024 00:00:00");
    rfc822_message_set_body(rfc_msg, "Test message body");
    
    /* Convert to FTN */
    if (rfc822_to_ftn(rfc_msg, "fidonet.org", &ftn_msg) != FTN_OK) {
        rfc822_message_free(rfc_msg);
        return 0;
    }
    
    if (!ftn_msg) {
        rfc822_message_free(rfc_msg);
        return 0;
    }
    
    /* Check addresses */
    if (ftn_msg->orig_addr.zone != 1 || ftn_msg->orig_addr.net != 2 ||
        ftn_msg->orig_addr.node != 3 || ftn_msg->orig_addr.point != 0) {
        ftn_message_free(ftn_msg);
        rfc822_message_free(rfc_msg);
        return 0;
    }
    
    if (ftn_msg->dest_addr.zone != 1 || ftn_msg->dest_addr.net != 2 ||
        ftn_msg->dest_addr.node != 4 || ftn_msg->dest_addr.point != 0) {
        ftn_message_free(ftn_msg);
        rfc822_message_free(rfc_msg);
        return 0;
    }
    
    /* Check names */
    if (!ftn_msg->from_user || strcmp(ftn_msg->from_user, "John Doe") != 0) {
        ftn_message_free(ftn_msg);
        rfc822_message_free(rfc_msg);
        return 0;
    }
    
    if (!ftn_msg->to_user || strcmp(ftn_msg->to_user, "Jane Smith") != 0) {
        ftn_message_free(ftn_msg);
        rfc822_message_free(rfc_msg);
        return 0;
    }
    
    /* Check subject */
    if (!ftn_msg->subject || strcmp(ftn_msg->subject, "Test Subject") != 0) {
        ftn_message_free(ftn_msg);
        rfc822_message_free(rfc_msg);
        return 0;
    }
    
    /* Check body */
    if (!ftn_msg->text || strcmp(ftn_msg->text, "Test message body") != 0) {
        ftn_message_free(ftn_msg);
        rfc822_message_free(rfc_msg);
        return 0;
    }
    
    ftn_message_free(ftn_msg);
    rfc822_message_free(rfc_msg);
    return 1;
}

/* Test roundtrip conversion (FTN -> RFC822 -> FTN) */
static int test_roundtrip_conversion(void) {
    ftn_message_t* original;
    rfc822_message_t* rfc_msg = NULL;
    ftn_message_t* converted = NULL;
    int result;
    
    original = ftn_message_new(FTN_MSG_NETMAIL);
    if (!original) return 0;
    
    /* Set up original FTN message */
    original->orig_addr.zone = 1;
    original->orig_addr.net = 2;
    original->orig_addr.node = 3;
    original->orig_addr.point = 4;
    
    original->dest_addr.zone = 1;
    original->dest_addr.net = 2;
    original->dest_addr.node = 5;
    original->dest_addr.point = 0;
    
    original->from_user = malloc(strlen("Sender Name") + 1);
    strcpy(original->from_user, "Sender Name");
    
    original->to_user = malloc(strlen("Recipient Name") + 1);
    strcpy(original->to_user, "Recipient Name");
    
    original->subject = malloc(strlen("Roundtrip Test") + 1);
    strcpy(original->subject, "Roundtrip Test");
    
    original->text = malloc(strlen("This is a roundtrip test message.") + 1);
    strcpy(original->text, "This is a roundtrip test message.");
    
    original->timestamp = 1704067200;
    original->attributes = FTN_ATTR_PRIVATE | FTN_ATTR_CRASH;
    
    /* Convert FTN -> RFC822 */
    if (ftn_to_rfc822(original, "fidonet.org", &rfc_msg) != FTN_OK) {
        ftn_message_free(original);
        return 0;
    }
    
    /* Convert RFC822 -> FTN */
    if (rfc822_to_ftn(rfc_msg, "fidonet.org", &converted) != FTN_OK) {
        rfc822_message_free(rfc_msg);
        ftn_message_free(original);
        return 0;
    }
    
    /* Compare key fields */
    result = 1;
    
    if (converted->orig_addr.zone != original->orig_addr.zone ||
        converted->orig_addr.net != original->orig_addr.net ||
        converted->orig_addr.node != original->orig_addr.node ||
        converted->orig_addr.point != original->orig_addr.point) {
        result = 0;
    }
    
    if (converted->dest_addr.zone != original->dest_addr.zone ||
        converted->dest_addr.net != original->dest_addr.net ||
        converted->dest_addr.node != original->dest_addr.node ||
        converted->dest_addr.point != original->dest_addr.point) {
        result = 0;
    }
    
    if (!converted->from_user || !original->from_user ||
        strcmp(converted->from_user, original->from_user) != 0) {
        result = 0;
    }
    
    if (!converted->to_user || !original->to_user ||
        strcmp(converted->to_user, original->to_user) != 0) {
        result = 0;
    }
    
    if (!converted->subject || !original->subject ||
        strcmp(converted->subject, original->subject) != 0) {
        result = 0;
    }
    
    if (!converted->text || !original->text ||
        strcmp(converted->text, original->text) != 0) {
        result = 0;
    }
    
    ftn_message_free(original);
    ftn_message_free(converted);
    rfc822_message_free(rfc_msg);
    
    return result;
}

int main(void) {
    printf("Running RFC822 conversion library tests...\n\n");
    
    TEST(message_creation);
    TEST(message_parsing);
    TEST(message_generation);
    TEST(ftn_address_to_rfc822);
    TEST(rfc822_address_to_ftn);
    TEST(ftn_to_rfc822_conversion);
    TEST(rfc822_to_ftn_conversion);
    TEST(roundtrip_conversion);
    
    printf("\nTest Results: %d/%d tests passed\n", test_passed, test_count);
    
    return (test_passed == test_count) ? 0 : 1;
}