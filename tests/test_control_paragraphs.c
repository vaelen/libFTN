/*
 * Unit tests for FTN control paragraphs (Phase 3)
 * Tests FTS-4000, FTS-4001, FTS-4008, and FTS-4009 support
 */

#include "../include/ftn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test FTS-4000: Generic Control Paragraphs */
static void test_control_paragraphs(void) {
    ftn_message_t* message;
    ftn_error_t result;
    const char* value;
    
    printf("Testing FTS-4000 control paragraphs...\n");
    
    message = ftn_message_new(FTN_MSG_NETMAIL);
    assert(message != NULL);
    
    /* Test adding control paragraphs */
    result = ftn_message_add_control(message, "PID: TestProg 1.0");
    assert(result == FTN_OK);
    
    result = ftn_message_add_control(message, "TID: TestMail 2.0");
    assert(result == FTN_OK);
    
    result = ftn_message_add_control(message, "CHRS: UTF-8 4");
    assert(result == FTN_OK);
    
    /* Test retrieving control paragraphs */
    value = ftn_message_get_control(message, "PID");
    assert(value != NULL);
    assert(strstr(value, "TestProg 1.0") != NULL);
    
    value = ftn_message_get_control(message, "TID");
    assert(value != NULL);
    assert(strstr(value, "TestMail 2.0") != NULL);
    
    value = ftn_message_get_control(message, "CHRS");
    assert(value != NULL);
    assert(strstr(value, "UTF-8 4") != NULL);
    
    /* Test non-existent control paragraph */
    value = ftn_message_get_control(message, "NONEXISTENT");
    assert(value == NULL);
    
    /* Test control count */
    assert(message->control_count == 3);
    
    ftn_message_free(message);
    printf("FTS-4000 control paragraphs: PASSED\n");
}

/* Test FTS-4001: Addressing Control Paragraphs */
static void test_addressing_control_paragraphs(void) {
    ftn_message_t* message;
    ftn_address_t dest_addr, orig_addr;
    ftn_error_t result;
    
    printf("Testing FTS-4001 addressing control paragraphs...\n");
    
    message = ftn_message_new(FTN_MSG_NETMAIL);
    assert(message != NULL);
    
    /* Test FMPT and TOPT */
    result = ftn_message_set_fmpt(message, 123);
    assert(result == FTN_OK);
    assert(message->fmpt == 123);
    
    result = ftn_message_set_topt(message, 456);
    assert(result == FTN_OK);
    assert(message->topt == 456);
    
    /* Test INTL */
    dest_addr.zone = 2; dest_addr.net = 345; dest_addr.node = 6; dest_addr.point = 7;
    orig_addr.zone = 1; orig_addr.net = 123; orig_addr.node = 4; orig_addr.point = 5;
    
    result = ftn_message_set_intl(message, &dest_addr, &orig_addr);
    assert(result == FTN_OK);
    assert(message->intl != NULL);
    assert(strcmp(message->intl, "2:345/6 1:123/4") == 0);
    
    ftn_message_free(message);
    printf("FTS-4001 addressing control paragraphs: PASSED\n");
}

/* Test FTS-4008: Time Zone Information */
static void test_timezone_support(void) {
    ftn_message_t* message;
    ftn_error_t result;
    
    printf("Testing FTS-4008 time zone support...\n");
    
    message = ftn_message_new(FTN_MSG_NETMAIL);
    assert(message != NULL);
    
    /* Test TZUTC setting */
    result = ftn_message_set_tzutc(message, "-0400");
    assert(result == FTN_OK);
    assert(message->tzutc != NULL);
    assert(strcmp(message->tzutc, "-0400") == 0);
    
    /* Test updating TZUTC */
    result = ftn_message_set_tzutc(message, "+0200");
    assert(result == FTN_OK);
    assert(strcmp(message->tzutc, "+0200") == 0);
    
    /* Test UTC */
    result = ftn_message_set_tzutc(message, "0000");
    assert(result == FTN_OK);
    assert(strcmp(message->tzutc, "0000") == 0);
    
    ftn_message_free(message);
    printf("FTS-4008 time zone support: PASSED\n");
}

/* Test FTS-4009: Netmail Tracking */
static void test_netmail_tracking(void) {
    ftn_message_t* message;
    ftn_address_t addr;
    ftn_error_t result;
    
    printf("Testing FTS-4009 netmail tracking...\n");
    
    message = ftn_message_new(FTN_MSG_NETMAIL);
    assert(message != NULL);
    
    /* Test adding Via lines */
    addr.zone = 1; addr.net = 123; addr.node = 4; addr.point = 0;
    result = ftn_message_add_via(message, &addr, "20250815.123045.UTC", "TestMail", "1.0");
    assert(result == FTN_OK);
    assert(message->via_count == 1);
    assert(message->via_lines[0] != NULL);
    assert(strstr(message->via_lines[0], "1:123/4") != NULL);
    assert(strstr(message->via_lines[0], "20250815.123045.UTC") != NULL);
    assert(strstr(message->via_lines[0], "TestMail") != NULL);
    assert(strstr(message->via_lines[0], "1.0") != NULL);
    
    /* Test adding second Via line */
    addr.zone = 2; addr.net = 345; addr.node = 6; addr.point = 0;
    result = ftn_message_add_via(message, &addr, "20250815.124500.UTC", "Router", "2.1");
    assert(result == FTN_OK);
    assert(message->via_count == 2);
    assert(strstr(message->via_lines[1], "2:345/6") != NULL);
    
    ftn_message_free(message);
    printf("FTS-4009 netmail tracking: PASSED\n");
}

/* Test parsing control paragraphs from message text */
static void test_control_paragraph_parsing(void) {
    ftn_message_t* message;
    ftn_error_t result;
    const char* test_text = 
        "AREA:TEST.ECHO\r"
        "\001INTL 2:345/6 1:123/4\r"
        "\001FMPT 10\r"
        "\001TOPT 20\r"
        "\001TZUTC: -0500\r"
        "\001PID: TestParser 1.5\r"
        "\001MSGID: 1:123/4 abcd1234\r"
        "This is the message body.\r"
        "It has multiple lines.\r"
        "--- TestMail 1.0\r"
        " * Origin: Test System (1:123/4)\r"
        "SEEN-BY: 123/4 345/6\r"
        "\001PATH: 123/4\r"
        "\001Via 1:123/4 @20250815.120000.UTC TestRouter 1.0\r";
    
    printf("Testing control paragraph parsing from text...\n");
    
    message = ftn_message_new(FTN_MSG_NETMAIL);
    assert(message != NULL);
    
    result = ftn_message_parse_text(message, test_text);
    assert(result == FTN_OK);
    
    /* Check echomail detection */
    assert(message->type == FTN_MSG_ECHOMAIL);
    assert(message->area != NULL);
    assert(strcmp(message->area, "TEST.ECHO") == 0);
    
    /* Check FTS-4001: Addressing */
    assert(message->intl != NULL);
    assert(strcmp(message->intl, "2:345/6 1:123/4") == 0);
    assert(message->fmpt == 10);
    assert(message->topt == 20);
    
    /* Check FTS-4008: Time Zone */
    assert(message->tzutc != NULL);
    assert(strcmp(message->tzutc, "-0500") == 0);
    
    /* Check FTS-4000: Generic Control */
    {
        const char* pid;
        assert(message->control_count >= 1);
        pid = ftn_message_get_control(message, "PID");
        assert(pid != NULL);
        assert(strstr(pid, "TestParser 1.5") != NULL);
    }
    
    /* Check FTS-4009: Via tracking */
    assert(message->via_count >= 1);
    assert(strstr(message->via_lines[0], "1:123/4") != NULL);
    assert(strstr(message->via_lines[0], "TestRouter") != NULL);
    
    /* Check message text is clean (no control lines) */
    assert(message->text != NULL);
    assert(strstr(message->text, "This is the message body.") != NULL);
    assert(strstr(message->text, "\001") == NULL); /* No control chars in body */
    
    ftn_message_free(message);
    printf("Control paragraph parsing from text: PASSED\n");
}

/* Test message text creation with control paragraphs */
static void test_control_paragraph_creation(void) {
    ftn_message_t* message;
    ftn_address_t addr;
    char* text;
    
    printf("Testing control paragraph creation in message text...\n");
    
    message = ftn_message_new(FTN_MSG_NETMAIL);
    assert(message != NULL);
    
    /* Set up message with control paragraphs */
    message->to_user = strdup("Test User");
    message->from_user = strdup("Test Sender");
    message->subject = strdup("Test Message");
    message->text = strdup("This is a test message body.");
    
    /* Add control paragraphs */
    {
        ftn_address_t orig_addr = {1, 123, 4, 0};
        addr.zone = 2; addr.net = 345; addr.node = 6; addr.point = 0;
        ftn_message_set_intl(message, &addr, &orig_addr);
        ftn_message_set_fmpt(message, 5);
        ftn_message_set_topt(message, 10);
        ftn_message_set_tzutc(message, "-0800");
        ftn_message_add_control(message, "PID: TestCreator 2.0");
        ftn_message_set_msgid(message, &orig_addr, "test123");
    }
    
    /* Create message text */
    text = ftn_message_create_text(message);
    assert(text != NULL);
    
    /* Verify control paragraphs are in the text */
    assert(strstr(text, "\001INTL 2:345/6 1:123/4") != NULL);
    assert(strstr(text, "\001FMPT 5") != NULL);
    assert(strstr(text, "\001TOPT 10") != NULL);
    assert(strstr(text, "\001TZUTC: -0800") != NULL);
    assert(strstr(text, "\001PID: TestCreator 2.0") != NULL);
    assert(strstr(text, "\001MSGID: 1:123/4 test123") != NULL);
    
    /* Verify message body is included */
    assert(strstr(text, "This is a test message body.") != NULL);
    
    free(text);
    ftn_message_free(message);
    printf("Control paragraph creation in message text: PASSED\n");
}

/* Test roundtrip: create message with control paragraphs, convert to text, parse back */
static void test_control_paragraph_roundtrip(void) {
    ftn_message_t* original;
    ftn_message_t* parsed;
    ftn_address_t addr;
    char* text;
    ftn_error_t result;
    
    printf("Testing control paragraph roundtrip...\n");
    
    /* Create original message */
    original = ftn_message_new(FTN_MSG_NETMAIL);
    assert(original != NULL);
    
    original->text = strdup("Test message for roundtrip.");
    
    /* Add various control paragraphs */
    {
        ftn_address_t orig_addr = {2, 234, 5, 0};
        addr.zone = 3; addr.net = 456; addr.node = 7; addr.point = 0;
        ftn_message_set_intl(original, &addr, &orig_addr);
        ftn_message_set_fmpt(original, 15);
        ftn_message_set_topt(original, 25);
        ftn_message_set_tzutc(original, "+0300");
        ftn_message_add_control(original, "PID: RoundtripTest 1.0");
        ftn_message_add_control(original, "TID: TestLib 3.0");
        ftn_message_set_msgid(original, &orig_addr, "roundtrip456");
        ftn_message_add_via(original, &orig_addr, "20250815.150000.UTC", "TestVia", "1.0");
    }
    
    /* Convert to text */
    text = ftn_message_create_text(original);
    assert(text != NULL);
    
    /* Parse back */
    parsed = ftn_message_new(FTN_MSG_NETMAIL);
    assert(parsed != NULL);
    
    result = ftn_message_parse_text(parsed, text);
    assert(result == FTN_OK);
    
    /* Verify all control paragraphs survived the roundtrip */
    assert(parsed->intl != NULL);
    assert(strcmp(parsed->intl, original->intl) == 0);
    assert(parsed->fmpt == original->fmpt);
    assert(parsed->topt == original->topt);
    assert(parsed->tzutc != NULL);
    assert(strcmp(parsed->tzutc, original->tzutc) == 0);
    assert(parsed->msgid != NULL);
    assert(strcmp(parsed->msgid, original->msgid) == 0);
    assert(parsed->via_count == original->via_count);
    
    /* Verify control paragraphs are preserved */
    {
        const char* pid = ftn_message_get_control(parsed, "PID");
        const char* tid;
        assert(pid != NULL);
        assert(strstr(pid, "RoundtripTest 1.0") != NULL);
        
        tid = ftn_message_get_control(parsed, "TID");
        assert(tid != NULL);
        assert(strstr(tid, "TestLib 3.0") != NULL);
    }
    
    /* Verify message text is preserved */
    assert(parsed->text != NULL);
    assert(strstr(parsed->text, "Test message for roundtrip.") != NULL);
    
    free(text);
    ftn_message_free(original);
    ftn_message_free(parsed);
    printf("Control paragraph roundtrip: PASSED\n");
}

int main(void) {
    printf("Running control paragraph tests...\n\n");
    
    test_control_paragraphs();
    test_addressing_control_paragraphs();
    test_timezone_support();
    test_netmail_tracking();
    test_control_paragraph_parsing();
    test_control_paragraph_creation();
    test_control_paragraph_roundtrip();
    
    printf("\nAll control paragraph tests passed!\n");
    return 0;
}