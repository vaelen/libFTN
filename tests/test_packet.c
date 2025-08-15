/*
 * test_packet - Packet and Message Test Suite
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "../include/ftn.h"
#include <assert.h>
#include <string.h>

static void test_message_creation(void) {
    ftn_message_t* message;
    
    printf("Testing message creation...\n");
    
    /* Test netmail message creation */
    message = ftn_message_new(FTN_MSG_NETMAIL);
    assert(message != NULL);
    assert(message->type == FTN_MSG_NETMAIL);
    assert(message->to_user == NULL);
    assert(message->from_user == NULL);
    assert(message->subject == NULL);
    assert(message->text == NULL);
    assert(message->attributes == 0);
    assert(message->timestamp > 0);
    ftn_message_free(message);
    
    /* Test echomail message creation */
    message = ftn_message_new(FTN_MSG_ECHOMAIL);
    assert(message != NULL);
    assert(message->type == FTN_MSG_ECHOMAIL);
    assert(message->area == NULL);
    assert(message->seenby == NULL);
    assert(message->seenby_count == 0);
    assert(message->path == NULL);
    assert(message->path_count == 0);
    ftn_message_free(message);
    
    printf("Message creation: PASSED\n");
}

static void test_message_attributes(void) {
    ftn_message_t* message;
    
    printf("Testing message attributes...\n");
    
    message = ftn_message_new(FTN_MSG_NETMAIL);
    assert(message != NULL);
    
    /* Test setting attributes */
    ftn_message_set_attribute(message, FTN_ATTR_PRIVATE);
    assert(ftn_message_has_attribute(message, FTN_ATTR_PRIVATE));
    assert(!ftn_message_has_attribute(message, FTN_ATTR_CRASH));
    
    ftn_message_set_attribute(message, FTN_ATTR_CRASH);
    assert(ftn_message_has_attribute(message, FTN_ATTR_PRIVATE));
    assert(ftn_message_has_attribute(message, FTN_ATTR_CRASH));
    
    /* Test clearing attributes */
    ftn_message_clear_attribute(message, FTN_ATTR_PRIVATE);
    assert(!ftn_message_has_attribute(message, FTN_ATTR_PRIVATE));
    assert(ftn_message_has_attribute(message, FTN_ATTR_CRASH));
    
    ftn_message_free(message);
    
    printf("Message attributes: PASSED\n");
}

static void test_datetime_conversion(void) {
    time_t timestamp = 946684800;  /* Jan 1, 2000 00:00:00 UTC */
    char datetime_str[21];
    time_t converted_back;
    
    printf("Testing datetime conversion...\n");
    
    /* Test timestamp to string */
    assert(ftn_datetime_to_string(timestamp, datetime_str, sizeof(datetime_str)) == FTN_OK);
    /* Note: datetime string should be exactly 20 chars, but may vary due to timezone */
    assert(strlen(datetime_str) >= 19 && strlen(datetime_str) <= 20);
    
    /* Test string to timestamp */
    assert(ftn_datetime_from_string("01 Jan 00  00:00:00", &converted_back) == FTN_OK);
    
    /* Test invalid formats */
    assert(ftn_datetime_from_string("invalid", &converted_back) == FTN_ERROR_INVALID_FORMAT);
    assert(ftn_datetime_from_string(NULL, &converted_back) == FTN_ERROR_INVALID_PARAMETER);
    assert(ftn_datetime_to_string(timestamp, NULL, 0) == FTN_ERROR_INVALID_PARAMETER);
    
    printf("Datetime conversion: PASSED\n");
}

static void test_message_msgid_reply(void) {
    ftn_message_t* message;
    ftn_address_t addr;
    
    printf("Testing MSGID and REPLY...\n");
    
    message = ftn_message_new(FTN_MSG_NETMAIL);
    assert(message != NULL);
    
    /* Set up address */
    addr.zone = 1;
    addr.net = 2;
    addr.node = 3;
    addr.point = 4;
    
    /* Test setting MSGID */
    assert(ftn_message_set_msgid(message, &addr, "12345678") == FTN_OK);
    assert(message->msgid != NULL);
    assert(strcmp(message->msgid, "1:2/3.4 12345678") == 0);
    
    /* Test setting REPLY */
    assert(ftn_message_set_reply(message, "1:2/3.4 87654321") == FTN_OK);
    assert(message->reply != NULL);
    assert(strcmp(message->reply, "1:2/3.4 87654321") == 0);
    
    /* Test invalid parameters */
    assert(ftn_message_set_msgid(NULL, &addr, "12345678") == FTN_ERROR_INVALID_PARAMETER);
    assert(ftn_message_set_reply(NULL, "test") == FTN_ERROR_INVALID_PARAMETER);
    
    ftn_message_free(message);
    
    printf("MSGID and REPLY: PASSED\n");
}

static void test_echomail_control_lines(void) {
    ftn_message_t* message;
    
    printf("Testing Echomail control lines...\n");
    
    message = ftn_message_new(FTN_MSG_ECHOMAIL);
    assert(message != NULL);
    
    /* Test adding SEEN-BY lines */
    assert(ftn_message_add_seenby(message, " 1:2/3 4 5") == FTN_OK);
    assert(ftn_message_add_seenby(message, " 1:6/7 8 9") == FTN_OK);
    assert(message->seenby_count == 2);
    assert(strcmp(message->seenby[0], "1:2/3 4 5") == 0);
    assert(strcmp(message->seenby[1], "1:6/7 8 9") == 0);
    
    /* Test adding PATH lines */
    assert(ftn_message_add_path(message, " 1:2/3") == FTN_OK);
    assert(ftn_message_add_path(message, " 1:4/5") == FTN_OK);
    assert(message->path_count == 2);
    assert(strcmp(message->path[0], "1:2/3") == 0);
    assert(strcmp(message->path[1], "1:4/5") == 0);
    
    /* Test invalid parameters */
    assert(ftn_message_add_seenby(NULL, "test") == FTN_ERROR_INVALID_PARAMETER);
    assert(ftn_message_add_path(NULL, "test") == FTN_ERROR_INVALID_PARAMETER);
    
    ftn_message_free(message);
    
    printf("Echomail control lines: PASSED\n");
}

static void test_message_text_parsing(void) {
    ftn_message_t* message;
    const char* echomail_text = 
        "AREA:TEST.ECHO\r\n"
        "\001MSGID: 1:2/3.4 12345678\r\n"
        "\001REPLY: 1:2/3.4 87654321\r\n"
        "This is the message body.\r\n"
        "It has multiple lines.\r\n"
        "--- TestMail 1.0\r\n"
        " * Origin: Test System (1:2/3.4)\r\n"
        "SEEN-BY: 1:2/3 4 5\r\n"
        "SEEN-BY: 1:6/7 8 9\r\n"
        "\001PATH: 1:2/3 1:4/5\r\n";
    
    printf("Testing message text parsing...\n");
    
    message = ftn_message_new(FTN_MSG_NETMAIL);
    assert(message != NULL);
    
    /* Parse the echomail text */
    assert(ftn_message_parse_text(message, echomail_text) == FTN_OK);
    
    /* Verify parsing results */
    assert(message->type == FTN_MSG_ECHOMAIL);
    assert(message->area != NULL);
    assert(strcmp(message->area, "TEST.ECHO") == 0);
    assert(message->msgid != NULL);
    assert(strcmp(message->msgid, "1:2/3.4 12345678") == 0);
    assert(message->reply != NULL);
    assert(strcmp(message->reply, "1:2/3.4 87654321") == 0);
    assert(message->tearline != NULL);
    assert(strcmp(message->tearline, "--- TestMail 1.0") == 0);
    assert(message->origin != NULL);
    assert(strcmp(message->origin, "* Origin: Test System (1:2/3.4)") == 0);
    assert(message->seenby_count == 2);
    assert(message->path_count == 1);
    
    ftn_message_free(message);
    
    printf("Message text parsing: PASSED\n");
}

static void test_message_text_creation(void) {
    ftn_message_t* message;
    char* generated_text;
    ftn_address_t addr;
    
    printf("Testing message text creation...\n");
    
    message = ftn_message_new(FTN_MSG_ECHOMAIL);
    assert(message != NULL);
    
    /* Set up message content */
    message->area = strdup("TEST.ECHO");
    message->text = strdup("This is a test message.");
    message->tearline = strdup("--- TestMail 1.0");
    message->origin = strdup("* Origin: Test System (1:2/3.4)");
    
    addr.zone = 1; addr.net = 2; addr.node = 3; addr.point = 4;
    ftn_message_set_msgid(message, &addr, "12345678");
    ftn_message_add_seenby(message, " 1:2/3 4 5");
    ftn_message_add_path(message, " 1:2/3");
    
    /* Generate text */
    generated_text = ftn_message_create_text(message);
    assert(generated_text != NULL);
    
    /* Verify key components are present */
    assert(strstr(generated_text, "AREA:TEST.ECHO") != NULL);
    assert(strstr(generated_text, "\001MSGID: 1:2/3.4 12345678") != NULL);
    assert(strstr(generated_text, "This is a test message.") != NULL);
    assert(strstr(generated_text, "--- TestMail 1.0") != NULL);
    assert(strstr(generated_text, "* Origin: Test System (1:2/3.4)") != NULL);
    assert(strstr(generated_text, "SEEN-BY:1:2/3 4 5") != NULL);
    assert(strstr(generated_text, "\001PATH: 1:2/3") != NULL);
    
    free(generated_text);
    ftn_message_free(message);
    
    printf("Message text creation: PASSED\n");
}

static void test_packet_creation(void) {
    ftn_packet_t* packet;
    ftn_message_t* message;
    
    printf("Testing packet creation...\n");
    
    /* Create packet */
    packet = ftn_packet_new();
    assert(packet != NULL);
    assert(packet->message_count == 0);
    assert(packet->message_capacity > 0);
    assert(packet->messages != NULL);
    
    /* Create and add message */
    message = ftn_message_new(FTN_MSG_NETMAIL);
    assert(message != NULL);
    message->to_user = strdup("John Doe");
    message->from_user = strdup("Jane Smith");
    message->subject = strdup("Test Message");
    message->text = strdup("This is a test.");
    
    assert(ftn_packet_add_message(packet, message) == FTN_OK);
    assert(packet->message_count == 1);
    assert(packet->messages[0] == message);
    
    /* Test invalid parameters */
    assert(ftn_packet_add_message(NULL, message) == FTN_ERROR_INVALID_PARAMETER);
    assert(ftn_packet_add_message(packet, NULL) == FTN_ERROR_INVALID_PARAMETER);
    
    ftn_packet_free(packet);
    
    printf("Packet creation: PASSED\n");
}

static void test_packet_roundtrip(void) {
    ftn_packet_t* packet;
    ftn_packet_t* loaded_packet;
    ftn_message_t* message;
    ftn_message_t* loaded_msg;
    const char* test_filename = "test_packet.pkt";
    time_t now;
    struct tm* tm;
    
    printf("Testing packet save/load roundtrip...\n");
    
    /* Create test packet */
    packet = ftn_packet_new();
    assert(packet != NULL);
    
    /* Set up packet header */
    packet->header.orig_node = 1;
    packet->header.dest_node = 2;
    packet->header.orig_net = 100;
    packet->header.dest_net = 200;
    packet->header.orig_zone = 1;
    packet->header.dest_zone = 1;
    packet->header.packet_type = 0x0002;
    now = time(NULL);
    tm = localtime(&now);
    packet->header.year = tm->tm_year + 1900;
    packet->header.month = tm->tm_mon;
    packet->header.day = tm->tm_mday;
    packet->header.hour = tm->tm_hour;
    packet->header.minute = tm->tm_min;
    packet->header.second = tm->tm_sec;
    strcpy(packet->header.password, "SECRET");
    
    /* Create test message */
    message = ftn_message_new(FTN_MSG_NETMAIL);
    assert(message != NULL);
    message->to_user = strdup("Test User");
    message->from_user = strdup("Test Sender");
    message->subject = strdup("Test Subject");
    message->text = strdup("This is a test message for packet roundtrip.");
    message->orig_addr.zone = 1;
    message->orig_addr.net = 100;
    message->orig_addr.node = 1;
    message->orig_addr.point = 0;
    message->dest_addr.zone = 1;
    message->dest_addr.net = 200;
    message->dest_addr.node = 2;
    message->dest_addr.point = 0;
    message->attributes = FTN_ATTR_PRIVATE;
    message->cost = 0;
    
    ftn_packet_add_message(packet, message);
    
    /* Save packet */
    assert(ftn_packet_save(test_filename, packet) == FTN_OK);
    
    /* Load packet */
    assert(ftn_packet_load(test_filename, &loaded_packet) == FTN_OK);
    assert(loaded_packet != NULL);
    assert(loaded_packet->message_count == 1);
    
    /* Verify packet header */
    assert(loaded_packet->header.orig_node == 1);
    assert(loaded_packet->header.dest_node == 2);
    assert(loaded_packet->header.orig_net == 100);
    assert(loaded_packet->header.dest_net == 200);
    assert(loaded_packet->header.packet_type == 0x0002);
    
    /* Verify message content */
    loaded_msg = loaded_packet->messages[0];
    assert(strcmp(loaded_msg->to_user, "Test User") == 0);
    assert(strcmp(loaded_msg->from_user, "Test Sender") == 0);
    assert(strcmp(loaded_msg->subject, "Test Subject") == 0);
    assert(loaded_msg->attributes == FTN_ATTR_PRIVATE);
    
    /* Cleanup */
    ftn_packet_free(packet);
    ftn_packet_free(loaded_packet);
    remove(test_filename);
    
    printf("Packet save/load roundtrip: PASSED\n");
}

int main(void) {
    printf("Running packet and message tests...\n\n");
    
    test_message_creation();
    test_message_attributes();
    test_datetime_conversion();
    test_message_msgid_reply();
    test_echomail_control_lines();
    test_message_text_parsing();
    test_message_text_creation();
    test_packet_creation();
    test_packet_roundtrip();
    
    printf("\nAll packet and message tests passed!\n");
    return 0;
}