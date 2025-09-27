/*
 * test_dupecheck.c - Duplicate detection system tests
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "ftn.h"
#include "ftn/dupecheck.h"
#include "ftn/packet.h"

static int tests_run = 0;
static int tests_passed = 0;

void test_start(const char* test_name) {
    printf("Testing %s... ", test_name);
    fflush(stdout);
}

void test_pass(void) {
    printf("PASS\n");
    tests_passed++;
    tests_run++;
}

void test_fail(const char* message) {
    printf("FAIL: %s\n", message);
    tests_run++;
}

/* Helper function to create a test message with MSGID */
ftn_message_t* create_test_message_with_msgid(const char* msgid) {
    ftn_message_t* msg = ftn_message_new(FTN_MSG_ECHOMAIL);
    char msgid_line[256];

    if (!msg) return NULL;

    /* Add MSGID control line */
    snprintf(msgid_line, sizeof(msgid_line), "MSGID: %s", msgid);
    if (ftn_message_add_control(msg, msgid_line) != FTN_OK) {
        ftn_message_free(msg);
        return NULL;
    }

    return msg;
}

/* Test MSGID extraction */
void test_msgid_extraction_basic(void) {
    ftn_message_t* msg;
    char* msgid;

    test_start("MSGID extraction basic");

    msg = create_test_message_with_msgid("1:2/3@fidonet 12345678");
    if (!msg) {
        test_fail("Failed to create test message");
        return;
    }

    msgid = ftn_dupecheck_extract_msgid(msg);
    if (!msgid || strcmp(msgid, "1:2/3@fidonet 12345678") != 0) {
        test_fail("Failed to extract MSGID");
        if (msgid) free(msgid);
        ftn_message_free(msg);
        return;
    }

    free(msgid);
    ftn_message_free(msg);
    test_pass();
}

void test_msgid_extraction_no_msgid(void) {
    ftn_message_t* msg;
    char* msgid;

    test_start("MSGID extraction with no MSGID");

    msg = ftn_message_new(FTN_MSG_ECHOMAIL);
    if (!msg) {
        test_fail("Failed to create test message");
        return;
    }

    msgid = ftn_dupecheck_extract_msgid(msg);
    if (msgid != NULL) {
        test_fail("Should return NULL for message without MSGID");
        free(msgid);
        ftn_message_free(msg);
        return;
    }

    ftn_message_free(msg);
    test_pass();
}

void test_msgid_extraction_case_insensitive(void) {
    ftn_message_t* msg;
    char* msgid;

    test_start("MSGID extraction case insensitive");

    msg = ftn_message_new(FTN_MSG_ECHOMAIL);
    if (!msg) {
        test_fail("Failed to create test message");
        return;
    }

    /* Add lowercase msgid line */
    if (ftn_message_add_control(msg, "msgid: 1:2/3@fidonet abcdef") != FTN_OK) {
        test_fail("Failed to add control line");
        ftn_message_free(msg);
        return;
    }

    msgid = ftn_dupecheck_extract_msgid(msg);
    if (!msgid || strcmp(msgid, "1:2/3@fidonet abcdef") != 0) {
        test_fail("Failed to extract MSGID case insensitive");
        if (msgid) free(msgid);
        ftn_message_free(msg);
        return;
    }

    free(msgid);
    ftn_message_free(msg);
    test_pass();
}

void test_msgid_extraction_whitespace(void) {
    ftn_message_t* msg;
    char* msgid;

    test_start("MSGID extraction with whitespace");

    msg = ftn_message_new(FTN_MSG_ECHOMAIL);
    if (!msg) {
        test_fail("Failed to create test message");
        return;
    }

    /* Add MSGID with extra whitespace */
    if (ftn_message_add_control(msg, "MSGID:   1:2/3@fidonet 12345678   ") != FTN_OK) {
        test_fail("Failed to add control line");
        ftn_message_free(msg);
        return;
    }

    msgid = ftn_dupecheck_extract_msgid(msg);
    if (!msgid || strcmp(msgid, "1:2/3@fidonet 12345678") != 0) {
        test_fail("Failed to trim whitespace from MSGID");
        if (msgid) free(msgid);
        ftn_message_free(msg);
        return;
    }

    free(msgid);
    ftn_message_free(msg);
    test_pass();
}

/* Test MSGID normalization */
void test_msgid_normalization(void) {
    char* normalized;

    test_start("MSGID normalization");

    normalized = ftn_dupecheck_normalize_msgid("1:2/3@FIDONET 12345678");
    if (!normalized || strcmp(normalized, "1:2/3@fidonet 12345678") != 0) {
        test_fail("Failed to normalize MSGID to lowercase");
        if (normalized) free(normalized);
        return;
    }
    free(normalized);

    /* Test whitespace normalization */
    normalized = ftn_dupecheck_normalize_msgid("1:2/3@fidonet    12345678");
    if (!normalized || strcmp(normalized, "1:2/3@fidonet 12345678") != 0) {
        test_fail("Failed to normalize whitespace in MSGID");
        if (normalized) free(normalized);
        return;
    }
    free(normalized);

    test_pass();
}

void test_msgid_validation(void) {
    test_start("MSGID validation");

    if (!ftn_dupecheck_is_valid_msgid("1:2/3@fidonet 12345678")) {
        test_fail("Valid MSGID rejected");
        return;
    }

    if (ftn_dupecheck_is_valid_msgid("")) {
        test_fail("Empty MSGID accepted");
        return;
    }

    if (ftn_dupecheck_is_valid_msgid("   ")) {
        test_fail("Whitespace-only MSGID accepted");
        return;
    }

    if (ftn_dupecheck_is_valid_msgid(NULL)) {
        test_fail("NULL MSGID accepted");
        return;
    }

    test_pass();
}

/* Test database operations */
void test_database_create_and_free(void) {
    ftn_dupecheck_t* dupecheck;

    test_start("database create and free");

    dupecheck = ftn_dupecheck_new("/tmp/test_dupecheck.db");
    if (!dupecheck) {
        test_fail("Failed to create dupecheck");
        return;
    }

    ftn_dupecheck_free(dupecheck);
    test_pass();
}

void test_database_add_and_find(void) {
    ftn_dupecheck_t* dupecheck;
    ftn_message_t* msg;
    int is_dupe;

    test_start("database add and find");

    dupecheck = ftn_dupecheck_new("/tmp/test_dupecheck_add.db");
    if (!dupecheck) {
        test_fail("Failed to create dupecheck");
        return;
    }

    if (ftn_dupecheck_load(dupecheck) != FTN_OK) {
        test_fail("Failed to load dupecheck database");
        ftn_dupecheck_free(dupecheck);
        return;
    }

    /* Create test message */
    msg = create_test_message_with_msgid("1:2/3@fidonet test123");
    if (!msg) {
        test_fail("Failed to create test message");
        ftn_dupecheck_free(dupecheck);
        return;
    }

    /* Should not be duplicate initially */
    if (ftn_dupecheck_is_duplicate(dupecheck, msg, &is_dupe) != FTN_OK || is_dupe) {
        test_fail("Message incorrectly identified as duplicate");
        ftn_message_free(msg);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    /* Add message to database */
    if (ftn_dupecheck_add_message(dupecheck, msg) != FTN_OK) {
        test_fail("Failed to add message to database");
        ftn_message_free(msg);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    /* Should now be duplicate */
    if (ftn_dupecheck_is_duplicate(dupecheck, msg, &is_dupe) != FTN_OK || !is_dupe) {
        test_fail("Message not identified as duplicate after adding");
        ftn_message_free(msg);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    ftn_message_free(msg);
    ftn_dupecheck_free(dupecheck);
    unlink("/tmp/test_dupecheck_add.db");
    test_pass();
}

void test_database_save_and_load(void) {
    ftn_dupecheck_t* dupecheck1, * dupecheck2;
    ftn_message_t* msg;
    int is_dupe;

    test_start("database save and load");

    /* Create first dupecheck and add a message */
    dupecheck1 = ftn_dupecheck_new("/tmp/test_dupecheck_save.db");
    if (!dupecheck1) {
        test_fail("Failed to create first dupecheck");
        return;
    }

    if (ftn_dupecheck_load(dupecheck1) != FTN_OK) {
        test_fail("Failed to load first dupecheck database");
        ftn_dupecheck_free(dupecheck1);
        return;
    }

    msg = create_test_message_with_msgid("1:2/3@fidonet savetest123");
    if (!msg) {
        test_fail("Failed to create test message");
        ftn_dupecheck_free(dupecheck1);
        return;
    }

    if (ftn_dupecheck_add_message(dupecheck1, msg) != FTN_OK) {
        test_fail("Failed to add message to first database");
        ftn_message_free(msg);
        ftn_dupecheck_free(dupecheck1);
        return;
    }

    /* Save the database */
    if (ftn_dupecheck_save(dupecheck1) != FTN_OK) {
        test_fail("Failed to save database");
        ftn_message_free(msg);
        ftn_dupecheck_free(dupecheck1);
        return;
    }

    ftn_dupecheck_free(dupecheck1);

    /* Create second dupecheck and load the saved database */
    dupecheck2 = ftn_dupecheck_new("/tmp/test_dupecheck_save.db");
    if (!dupecheck2) {
        test_fail("Failed to create second dupecheck");
        ftn_message_free(msg);
        return;
    }

    if (ftn_dupecheck_load(dupecheck2) != FTN_OK) {
        test_fail("Failed to load second dupecheck database");
        ftn_message_free(msg);
        ftn_dupecheck_free(dupecheck2);
        return;
    }

    /* Message should be found as duplicate */
    if (ftn_dupecheck_is_duplicate(dupecheck2, msg, &is_dupe) != FTN_OK || !is_dupe) {
        test_fail("Message not found as duplicate after reload");
        ftn_message_free(msg);
        ftn_dupecheck_free(dupecheck2);
        return;
    }

    ftn_message_free(msg);
    ftn_dupecheck_free(dupecheck2);
    unlink("/tmp/test_dupecheck_save.db");
    test_pass();
}

void test_database_cleanup_old(void) {
    ftn_dupecheck_t* dupecheck;
    ftn_message_t* msg1, * msg2;
    int is_dupe;
    time_t cutoff_time;

    test_start("database cleanup old entries");

    dupecheck = ftn_dupecheck_new("/tmp/test_dupecheck_cleanup.db");
    if (!dupecheck) {
        test_fail("Failed to create dupecheck");
        return;
    }

    if (ftn_dupecheck_load(dupecheck) != FTN_OK) {
        test_fail("Failed to load dupecheck database");
        ftn_dupecheck_free(dupecheck);
        return;
    }

    /* Add first message */
    msg1 = create_test_message_with_msgid("1:2/3@fidonet old123");
    if (!msg1) {
        test_fail("Failed to create first test message");
        ftn_dupecheck_free(dupecheck);
        return;
    }

    if (ftn_dupecheck_add_message(dupecheck, msg1) != FTN_OK) {
        test_fail("Failed to add first message");
        ftn_message_free(msg1);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    /* Wait a moment, then add second message */
    sleep(2);
    time(&cutoff_time);

    msg2 = create_test_message_with_msgid("1:2/3@fidonet new123");
    if (!msg2) {
        test_fail("Failed to create second test message");
        ftn_message_free(msg1);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    if (ftn_dupecheck_add_message(dupecheck, msg2) != FTN_OK) {
        test_fail("Failed to add second message");
        ftn_message_free(msg1);
        ftn_message_free(msg2);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    /* Both should be found as duplicates */
    if (ftn_dupecheck_is_duplicate(dupecheck, msg1, &is_dupe) != FTN_OK || !is_dupe) {
        test_fail("First message not found as duplicate before cleanup");
        ftn_message_free(msg1);
        ftn_message_free(msg2);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    if (ftn_dupecheck_is_duplicate(dupecheck, msg2, &is_dupe) != FTN_OK || !is_dupe) {
        test_fail("Second message not found as duplicate before cleanup");
        ftn_message_free(msg1);
        ftn_message_free(msg2);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    /* Cleanup old entries */
    if (ftn_dupecheck_cleanup_old(dupecheck, cutoff_time) != FTN_OK) {
        test_fail("Failed to cleanup old entries");
        ftn_message_free(msg1);
        ftn_message_free(msg2);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    /* First message should no longer be found, second should still be there */
    if (ftn_dupecheck_is_duplicate(dupecheck, msg1, &is_dupe) != FTN_OK || is_dupe) {
        test_fail("First message still found as duplicate after cleanup");
        ftn_message_free(msg1);
        ftn_message_free(msg2);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    if (ftn_dupecheck_is_duplicate(dupecheck, msg2, &is_dupe) != FTN_OK || !is_dupe) {
        test_fail("Second message not found as duplicate after cleanup");
        ftn_message_free(msg1);
        ftn_message_free(msg2);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    ftn_message_free(msg1);
    ftn_message_free(msg2);
    ftn_dupecheck_free(dupecheck);
    unlink("/tmp/test_dupecheck_cleanup.db");
    test_pass();
}

void test_database_statistics(void) {
    ftn_dupecheck_t* dupecheck;
    ftn_message_t* msg1, * msg2;
    ftn_dupecheck_stats_t stats;

    test_start("database statistics");

    dupecheck = ftn_dupecheck_new("/tmp/test_dupecheck_stats.db");
    if (!dupecheck) {
        test_fail("Failed to create dupecheck");
        return;
    }

    if (ftn_dupecheck_load(dupecheck) != FTN_OK) {
        test_fail("Failed to load dupecheck database");
        ftn_dupecheck_free(dupecheck);
        return;
    }

    /* Get initial stats */
    if (ftn_dupecheck_get_stats(dupecheck, &stats) != FTN_OK) {
        test_fail("Failed to get initial stats");
        ftn_dupecheck_free(dupecheck);
        return;
    }

    if (stats.total_entries != 0) {
        test_fail("Initial database should be empty");
        ftn_dupecheck_free(dupecheck);
        return;
    }

    /* Add messages */
    msg1 = create_test_message_with_msgid("1:2/3@fidonet stats1");
    msg2 = create_test_message_with_msgid("1:2/3@fidonet stats2");

    if (!msg1 || !msg2) {
        test_fail("Failed to create test messages");
        if (msg1) ftn_message_free(msg1);
        if (msg2) ftn_message_free(msg2);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    ftn_dupecheck_add_message(dupecheck, msg1);
    ftn_dupecheck_add_message(dupecheck, msg2);

    /* Get updated stats */
    if (ftn_dupecheck_get_stats(dupecheck, &stats) != FTN_OK) {
        test_fail("Failed to get updated stats");
        ftn_message_free(msg1);
        ftn_message_free(msg2);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    if (stats.total_entries != 2) {
        test_fail("Database should contain 2 entries");
        ftn_message_free(msg1);
        ftn_message_free(msg2);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    ftn_message_free(msg1);
    ftn_message_free(msg2);
    ftn_dupecheck_free(dupecheck);
    unlink("/tmp/test_dupecheck_stats.db");
    test_pass();
}

/* Test error conditions */
void test_error_conditions(void) {
    ftn_dupecheck_t* dupecheck;
    int is_dupe;

    test_start("error condition handling");

    /* Test invalid parameters */
    if (ftn_dupecheck_new(NULL) != NULL) {
        test_fail("Should fail with NULL db_path");
        return;
    }

    dupecheck = ftn_dupecheck_new("/tmp/test_dupecheck_error.db");
    if (!dupecheck) {
        test_fail("Failed to create dupecheck");
        return;
    }

    /* Test NULL message */
    if (ftn_dupecheck_is_duplicate(dupecheck, NULL, &is_dupe) == FTN_OK) {
        test_fail("Should fail with NULL message");
        ftn_dupecheck_free(dupecheck);
        return;
    }

    /* Test NULL is_dupe pointer */
    if (ftn_dupecheck_is_duplicate(dupecheck, NULL, NULL) == FTN_OK) {
        test_fail("Should fail with NULL is_dupe");
        ftn_dupecheck_free(dupecheck);
        return;
    }

    ftn_dupecheck_free(dupecheck);
    test_pass();
}

/* Test performance with large number of messages */
void test_performance_large_dataset(void) {
    ftn_dupecheck_t* dupecheck;
    ftn_message_t* msg;
    char msgid[64];
    int i;
    int is_dupe;
    time_t start_time, end_time;

    test_start("performance with large dataset");

    dupecheck = ftn_dupecheck_new("/tmp/test_dupecheck_performance.db");
    if (!dupecheck) {
        test_fail("Failed to create dupecheck");
        return;
    }

    if (ftn_dupecheck_load(dupecheck) != FTN_OK) {
        test_fail("Failed to load dupecheck database");
        ftn_dupecheck_free(dupecheck);
        return;
    }

    time(&start_time);

    /* Add 1000 messages */
    for (i = 0; i < 1000; i++) {
        snprintf(msgid, sizeof(msgid), "1:2/3@fidonet perf%d", i);
        msg = create_test_message_with_msgid(msgid);
        if (!msg) {
            test_fail("Failed to create test message");
            ftn_dupecheck_free(dupecheck);
            return;
        }

        if (ftn_dupecheck_add_message(dupecheck, msg) != FTN_OK) {
            test_fail("Failed to add message to database");
            ftn_message_free(msg);
            ftn_dupecheck_free(dupecheck);
            return;
        }

        ftn_message_free(msg);
    }

    /* Test lookup performance */
    snprintf(msgid, sizeof(msgid), "1:2/3@fidonet perf%d", 500);
    msg = create_test_message_with_msgid(msgid);
    if (!msg) {
        test_fail("Failed to create lookup test message");
        ftn_dupecheck_free(dupecheck);
        return;
    }

    if (ftn_dupecheck_is_duplicate(dupecheck, msg, &is_dupe) != FTN_OK || !is_dupe) {
        test_fail("Failed to find message in large dataset");
        ftn_message_free(msg);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    time(&end_time);

    /* Should complete in reasonable time (less than 10 seconds) */
    if (end_time - start_time > 10) {
        test_fail("Performance test took too long");
        ftn_message_free(msg);
        ftn_dupecheck_free(dupecheck);
        return;
    }

    ftn_message_free(msg);
    ftn_dupecheck_free(dupecheck);
    unlink("/tmp/test_dupecheck_performance.db");
    test_pass();
}

int main(void) {
    printf("Duplicate Detection System Tests\n");
    printf("================================\n\n");

    /* MSGID extraction tests */
    test_msgid_extraction_basic();
    test_msgid_extraction_no_msgid();
    test_msgid_extraction_case_insensitive();
    test_msgid_extraction_whitespace();

    /* MSGID normalization tests */
    test_msgid_normalization();
    test_msgid_validation();

    /* Database operation tests */
    test_database_create_and_free();
    test_database_add_and_find();
    test_database_save_and_load();
    test_database_cleanup_old();
    test_database_statistics();

    /* Error condition tests */
    test_error_conditions();

    /* Performance tests */
    test_performance_large_dataset();

    printf("\nTest Results: %d/%d tests passed\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("All tests PASSED!\n");
        return 0;
    } else {
        printf("Some tests FAILED!\n");
        return 1;
    }
}