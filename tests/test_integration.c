/*
 * test_integration.c - Integration tests for the complete FTN packet processing workflow
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "ftn.h"
#include "ftn/config.h"
#include "ftn/packet.h"
#include "ftn/router.h"
#include "ftn/storage.h"
#include "ftn/dupecheck.h"

/* Test configuration file content */
static const char* test_config_content =
"[node]\n"
"name=Test BBS\n"
"sysop=Test Sysop\n"
"location=Test Location\n"
"phone=555-1234\n"
"bbs_name=Test BBS System\n"
"networks=testnet\n"
"\n"
"[testnet]\n"
"name=testnet\n"
"domain=testnet.test\n"
"address=1:1/1.0\n"
"hub=1:1/100.0\n"
"inbox=tmp/testnet/inbox\n"
"outbox=tmp/testnet/outbox\n"
"processed=tmp/testnet/processed\n"
"bad=tmp/testnet/bad\n"
"duplicate_db=tmp/testnet/dupecheck.db\n"
"\n"
"[mail]\n"
"inbox=tmp/mail\n"
"\n"
"[news]\n"
"path=tmp/news\n";

/* Test results */
typedef struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
} test_results_t;

static test_results_t results = {0, 0, 0};

/* Test utility functions */
static void test_assert(int condition, const char* test_name) {
    results.tests_run++;
    if (condition) {
        printf("  %s: PASS\n", test_name);
        results.tests_passed++;
    } else {
        printf("  %s: FAIL\n", test_name);
        results.tests_failed++;
    }
}

static void cleanup_test_dirs(void) {
    int status;
    status = system("rm -rf tmp/testnet");
    status = system("rm -rf tmp/mail");
    status = system("rm -rf tmp/news");
    status = system("rm -f tmp/test_config.ini");
    (void)status;
}

static int setup_test_environment(void) {
    FILE* config_file;

    /* Create temp directory */
    if (mkdir("tmp", 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Failed to create tmp directory\n");
        return -1;
    }

    /* Create test configuration file */
    config_file = fopen("tmp/test_config.ini", "w");
    if (!config_file) {
        fprintf(stderr, "Failed to create test config file\n");
        return -1;
    }

    fprintf(config_file, "%s", test_config_content);
    fclose(config_file);

    return 0;
}

static ftn_packet_t* create_test_packet(void) {
    ftn_packet_t* packet;
    ftn_message_t* msg;
    ftn_address_t from_addr = {1, 1, 1, 0};
    ftn_address_t to_addr = {1, 1, 2, 0};

    packet = ftn_packet_new();
    if (!packet) return NULL;

    /* Create test netmail message */
    msg = ftn_message_new(FTN_MSG_NETMAIL);
    if (!msg) {
        ftn_packet_free(packet);
        return NULL;
    }

    msg->orig_addr = from_addr;
    msg->dest_addr = to_addr;
    msg->from_user = strdup("Test From");
    msg->to_user = strdup("Test To");
    msg->subject = strdup("Test Subject");
    msg->text = strdup("This is a test message.\r\n");
    msg->msgid = strdup("1@1:1/1.0 12345678");

    /* Add message to packet */
    ftn_packet_add_message(packet, msg);

    /* Create test echomail message */
    msg = ftn_message_new(FTN_MSG_ECHOMAIL);
    if (!msg) {
        ftn_packet_free(packet);
        return NULL;
    }

    msg->orig_addr = from_addr;
    msg->dest_addr = to_addr;
    msg->from_user = strdup("Test From");
    msg->to_user = strdup("All");
    msg->subject = strdup("Test Echo Subject");
    msg->text = strdup("This is a test echomail message.\r\n");
    msg->area = strdup("TEST.GENERAL");
    msg->msgid = strdup("2@1:1/1.0 12345679");

    /* Add message to packet */
    ftn_packet_add_message(packet, msg);

    return packet;
}

/* Test 1: Configuration loading and validation */
static void test_config_loading(void) {
    ftn_config_t* config;

    printf("Testing configuration loading and validation...\n");

    config = ftn_config_new();
    test_assert(config != NULL, "Config structure creation");

    if (config) {
        ftn_error_t error = ftn_config_load(config, "tmp/test_config.ini");
        test_assert(error == FTN_OK, "Configuration file loading");

        error = ftn_config_validate(config);
        test_assert(error == FTN_OK, "Configuration validation");

        test_assert(config->network_count == 1, "Network count correct");

        if (config->network_count > 0) {
            test_assert(strcmp(config->networks[0].name, "testnet") == 0, "Network name correct");
            test_assert(config->networks[0].inbox != NULL, "Inbox path configured");
        }

        ftn_config_free(config);
    }
}

/* Test 2: System initialization */
static void test_system_initialization(void) {
    ftn_config_t* config;
    ftn_router_t* router = NULL;
    ftn_storage_t* storage = NULL;
    ftn_dupecheck_t* dupecheck = NULL;

    printf("Testing system initialization...\n");

    config = ftn_config_new();
    if (config && ftn_config_load(config, "tmp/test_config.ini") == FTN_OK) {

        /* Test storage initialization */
        storage = ftn_storage_new(config);
        test_assert(storage != NULL, "Storage system creation");

        if (storage) {
            ftn_error_t error = ftn_storage_initialize(storage);
            test_assert(error == FTN_OK, "Storage system initialization");
        }

        /* Test dupecheck initialization */
        dupecheck = ftn_dupecheck_new("tmp/testnet/dupecheck.db");
        test_assert(dupecheck != NULL, "Dupecheck system creation");

        if (dupecheck) {
            ftn_error_t error = ftn_dupecheck_load(dupecheck);
            test_assert(error == FTN_OK, "Dupecheck system loading");
        }

        /* Test router initialization */
        if (dupecheck) {
            router = ftn_router_new(config, dupecheck);
            test_assert(router != NULL, "Router system creation");
        }

        /* Cleanup */
        if (router) ftn_router_free(router);
        if (storage) ftn_storage_free(storage);
        if (dupecheck) ftn_dupecheck_free(dupecheck);
        ftn_config_free(config);
    } else {
        test_assert(0, "Config loading failed - skipping system init tests");
    }
}

/* Test 3: Packet creation and processing */
static void test_packet_processing(void) {
    ftn_packet_t* packet;
    ftn_config_t* config;
    ftn_error_t error;
    ftn_packet_t* loaded_packet;

    printf("Testing packet creation and basic processing...\n");

    /* Create test packet */
    packet = create_test_packet();
    test_assert(packet != NULL, "Test packet creation");

    if (packet) {
        test_assert(packet->message_count == 2, "Packet message count correct");
        test_assert(packet->messages[0]->type == FTN_MSG_NETMAIL, "First message is netmail");
        test_assert(packet->messages[1]->type == FTN_MSG_ECHOMAIL, "Second message is echomail");

        /* Test packet save/load */
        loaded_packet = NULL;

        error = ftn_packet_save("tmp/test.pkt", packet);
        test_assert(error == FTN_OK, "Packet save");

        /* Load the packet back */
        error = ftn_packet_load("tmp/test.pkt", &loaded_packet);
        test_assert(error == FTN_OK, "Packet load");

        if (loaded_packet) {
            test_assert(loaded_packet->message_count == packet->message_count, "Loaded packet message count");
            ftn_packet_free(loaded_packet);
        }

        ftn_packet_free(packet);
    }

    /* Test configuration can be loaded for processing */
    config = ftn_config_new();
    if (config && ftn_config_load(config, "tmp/test_config.ini") == FTN_OK) {
        test_assert(1, "Configuration ready for processing");
        ftn_config_free(config);
    } else {
        test_assert(0, "Configuration not ready for processing");
    }
}

/* Test 4: Directory management */
static void test_directory_management(void) {
    ftn_config_t* config;
    struct stat st;
    ftn_packet_t* packet;
    ftn_error_t error;
    int status;

    printf("Testing directory management...\n");

    config = ftn_config_new();
    if (config && ftn_config_load(config, "tmp/test_config.ini") == FTN_OK) {

        /* Create a test packet in inbox */
        packet = create_test_packet();
        if (packet) {
            /* Create inbox directory manually first */
            status = system("mkdir -p tmp/testnet/inbox");
            (void)status;

            error = ftn_packet_save("tmp/testnet/inbox/test.pkt", packet);
            test_assert(error == FTN_OK, "Packet saved to inbox");

            /* Check file exists */
            test_assert(stat("tmp/testnet/inbox/test.pkt", &st) == 0, "Packet file exists in inbox");

            ftn_packet_free(packet);
        }

        ftn_config_free(config);
    } else {
        test_assert(0, "Configuration loading failed - skipping directory tests");
    }
}

/* Test 5: Error handling */
static void test_error_handling(void) {
    ftn_config_t* config;

    printf("Testing error handling...\n");

    /* Test loading non-existent config */
    config = ftn_config_new();
    if (config) {
        ftn_error_t error = ftn_config_load(config, "nonexistent.ini");
        test_assert(error != FTN_OK, "Non-existent config file properly rejected");
        ftn_config_free(config);
    }

    /* Test loading malformed packet */
    {
        FILE* bad_file = fopen("tmp/bad.pkt", "w");
        if (bad_file) {
            ftn_packet_t* packet = NULL;
            ftn_error_t error;

            fprintf(bad_file, "This is not a valid packet file\n");
            fclose(bad_file);

            error = ftn_packet_load("tmp/bad.pkt", &packet);
            test_assert(error != FTN_OK, "Malformed packet properly rejected");
            test_assert(packet == NULL, "No packet returned for malformed file");
        }
    }
}

/* Test 6: Memory management */
static void test_memory_management(void) {
    ftn_config_t* config;
    ftn_packet_t* packet;
    int i;

    printf("Testing memory management...\n");

    /* Test that we can create and free multiple objects */
    config = ftn_config_new();
    test_assert(config != NULL, "Config allocation");

    packet = ftn_packet_new();
    test_assert(packet != NULL, "Packet allocation");

    /* Add some messages to stress test */
    for (i = 0; i < 10; i++) {
        ftn_message_t* msg = ftn_message_new(FTN_MSG_NETMAIL);
        if (msg) {
            msg->from_user = strdup("Test User");
            msg->to_user = strdup("Test Recipient");
            msg->subject = strdup("Test Subject");
            msg->text = strdup("Test message text");
            ftn_packet_add_message(packet, msg);
        }
    }

    test_assert(packet->message_count == 10, "Multiple messages added");

    /* Cleanup should not crash */
    ftn_packet_free(packet);
    ftn_config_free(config);
    test_assert(1, "Memory cleanup completed without crash");
}

/* Main test runner */
int main(void) {
    printf("FTN Integration Tests\n");
    printf("=====================\n\n");

    /* Setup test environment */
    cleanup_test_dirs();
    if (setup_test_environment() != 0) {
        fprintf(stderr, "Failed to setup test environment\n");
        return 1;
    }

    /* Run tests */
    test_config_loading();
    test_system_initialization();
    test_packet_processing();
    test_directory_management();
    test_error_handling();
    test_memory_management();

    /* Cleanup */
    cleanup_test_dirs();

    /* Report results */
    printf("\nTest Summary:\n");
    printf("  Tests run: %d\n", results.tests_run);
    printf("  Tests passed: %d\n", results.tests_passed);
    printf("  Tests failed: %d\n", results.tests_failed);

    if (results.tests_failed == 0) {
        printf("\nAll tests PASSED!\n");
        return 0;
    } else {
        printf("\nSome tests FAILED!\n");
        return 1;
    }
}