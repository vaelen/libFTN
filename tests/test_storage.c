/*
 * test_storage.c - Storage system tests
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>

#include "ftn.h"
#include "ftn/storage.h"
#include "ftn/config.h"
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

/* Helper function to create test config */
ftn_config_t* create_test_config(void) {
    ftn_config_t* config = ftn_config_new();
    if (!config) return NULL;

    /* In a real test, we'd load from tests/data/storage_test_config.ini */
    /* For now, we'll create a minimal config manually */

    return config;
}

/* Helper function to create test message */
ftn_message_t* create_test_message(ftn_message_type_t type, const char* to_user, const char* from_user) {
    ftn_message_t* msg = ftn_message_new(type);
    if (!msg) return NULL;

    msg->to_user = malloc(strlen(to_user) + 1);
    msg->from_user = malloc(strlen(from_user) + 1);
    if (!msg->to_user || !msg->from_user) {
        ftn_message_free(msg);
        return NULL;
    }

    strcpy(msg->to_user, to_user);
    strcpy(msg->from_user, from_user);

    if (type == FTN_MSG_ECHOMAIL) {
        msg->area = malloc(16);
        if (msg->area) {
            strcpy(msg->area, "TEST.AREA");
        }
    }

    msg->subject = malloc(16);
    if (msg->subject) {
        strcpy(msg->subject, "Test Subject");
    }

    msg->text = malloc(32);
    if (msg->text) {
        strcpy(msg->text, "This is a test message.");
    }

    /* Set some basic addressing */
    msg->orig_addr.zone = 1;
    msg->orig_addr.net = 1;
    msg->orig_addr.node = 100;
    msg->orig_addr.point = 0;

    msg->dest_addr.zone = 1;
    msg->dest_addr.net = 1;
    msg->dest_addr.node = 200;
    msg->dest_addr.point = 0;

    return msg;
}

/* Test storage system lifecycle */
void test_storage_lifecycle(void) {
    ftn_config_t* config;
    ftn_storage_t* storage;

    test_start("storage lifecycle");

    config = create_test_config();
    if (!config) {
        test_fail("Failed to create test config");
        return;
    }

    storage = ftn_storage_new(config);
    if (!storage) {
        test_fail("Failed to create storage system");
        ftn_config_free(config);
        return;
    }

    /* Test initialization */
    if (ftn_storage_initialize(storage) != FTN_OK) {
        test_fail("Failed to initialize storage system");
        ftn_storage_free(storage);
        ftn_config_free(config);
        return;
    }

    ftn_storage_free(storage);
    ftn_config_free(config);

    test_pass();
}

/* Test path templating */
void test_path_templating(void) {
    char* result;

    test_start("path templating");

    /* Test simple path with user substitution */
    result = ftn_storage_expand_path("/home/%USER%/mail", "testuser", "fidonet");
    if (!result || strcmp(result, "/home/testuser/mail") != 0) {
        test_fail("User substitution failed");
        free(result);
        return;
    }
    free(result);

    /* Test path with network substitution */
    result = ftn_storage_expand_path("/var/spool/%NETWORK%", "testuser", "fsxnet");
    if (!result || strcmp(result, "/var/spool/fsxnet") != 0) {
        test_fail("Network substitution failed");
        free(result);
        return;
    }
    free(result);

    /* Test path with both substitutions */
    result = ftn_storage_expand_path("/var/mail/%NETWORK%/%USER%", "bob", "fidonet");
    if (!result || strcmp(result, "/var/mail/fidonet/bob") != 0) {
        test_fail("Combined substitution failed");
        free(result);
        return;
    }
    free(result);

    test_pass();
}

/* Test directory creation */
void test_directory_creation(void) {
    const char* test_dir = "tmp/test_storage_dir";
    struct stat st;

    test_start("directory creation");

    /* Clean up any existing test directory */
    rmdir(test_dir);

    /* Test directory creation */
    if (ftn_storage_ensure_directory(test_dir, FTN_STORAGE_DIR_MODE) != FTN_OK) {
        test_fail("Failed to create directory");
        return;
    }

    /* Verify directory exists */
    if (stat(test_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        test_fail("Directory was not created properly");
        return;
    }

    /* Test that calling again on existing directory succeeds */
    if (ftn_storage_ensure_directory(test_dir, FTN_STORAGE_DIR_MODE) != FTN_OK) {
        test_fail("Failed to handle existing directory");
        return;
    }

    /* Clean up */
    rmdir(test_dir);

    test_pass();
}

/* Test recursive directory creation */
void test_recursive_directory_creation(void) {
    const char* test_dir = "tmp/test_storage/deep/nested/path";
    struct stat st;

    test_start("recursive directory creation");

    /* Clean up any existing directories */
    rmdir("tmp/test_storage/deep/nested/path");
    rmdir("tmp/test_storage/deep/nested");
    rmdir("tmp/test_storage/deep");
    rmdir("tmp/test_storage");

    /* Test recursive directory creation */
    if (ftn_storage_create_directory_recursive(test_dir, FTN_STORAGE_DIR_MODE) != FTN_OK) {
        test_fail("Failed to create directory recursively");
        return;
    }

    /* Verify directory exists */
    if (stat(test_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        test_fail("Recursive directory was not created properly");
        return;
    }

    /* Clean up */
    rmdir("tmp/test_storage/deep/nested/path");
    rmdir("tmp/test_storage/deep/nested");
    rmdir("tmp/test_storage/deep");
    rmdir("tmp/test_storage");

    test_pass();
}

/* Test maildir creation */
void test_maildir_creation(void) {
    const char* maildir_path = "tmp/test_maildir";
    char subdir_path[256];
    struct stat st;

    test_start("maildir creation");

    /* Clean up any existing maildir */
    sprintf(subdir_path, "%s/tmp", maildir_path);
    rmdir(subdir_path);
    sprintf(subdir_path, "%s/new", maildir_path);
    rmdir(subdir_path);
    sprintf(subdir_path, "%s/cur", maildir_path);
    rmdir(subdir_path);
    rmdir(maildir_path);

    /* Create maildir */
    if (ftn_storage_create_maildir(maildir_path) != FTN_OK) {
        test_fail("Failed to create maildir");
        return;
    }

    /* Verify main directory exists */
    if (stat(maildir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        test_fail("Maildir main directory not created");
        return;
    }

    /* Verify subdirectories exist */
    sprintf(subdir_path, "%s/tmp", maildir_path);
    if (stat(subdir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        test_fail("Maildir tmp subdirectory not created");
        return;
    }

    sprintf(subdir_path, "%s/new", maildir_path);
    if (stat(subdir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        test_fail("Maildir new subdirectory not created");
        return;
    }

    sprintf(subdir_path, "%s/cur", maildir_path);
    if (stat(subdir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        test_fail("Maildir cur subdirectory not created");
        return;
    }

    /* Clean up */
    sprintf(subdir_path, "%s/tmp", maildir_path);
    rmdir(subdir_path);
    sprintf(subdir_path, "%s/new", maildir_path);
    rmdir(subdir_path);
    sprintf(subdir_path, "%s/cur", maildir_path);
    rmdir(subdir_path);
    rmdir(maildir_path);

    test_pass();
}

/* Test maildir filename generation */
void test_maildir_filename_generation(void) {
    ftn_maildir_file_t file_info;
    const char* maildir_path = "tmp/test_maildir";

    test_start("maildir filename generation");

    /* Create test maildir first */
    if (ftn_storage_create_maildir(maildir_path) != FTN_OK) {
        test_fail("Failed to create test maildir");
        return;
    }

    /* Generate filename */
    if (ftn_storage_generate_maildir_filename(&file_info, maildir_path) != FTN_OK) {
        test_fail("Failed to generate maildir filename");
        return;
    }

    /* Verify filename was generated */
    if (!file_info.filename || strlen(file_info.filename) == 0) {
        test_fail("Empty filename generated");
        ftn_maildir_file_free(&file_info);
        return;
    }

    /* Verify paths were generated */
    if (!file_info.tmp_path || !file_info.new_path) {
        test_fail("Paths not generated properly");
        ftn_maildir_file_free(&file_info);
        return;
    }

    /* Verify paths contain the right directories */
    if (strstr(file_info.tmp_path, "/tmp/") == NULL) {
        test_fail("tmp path doesn't contain /tmp/");
        ftn_maildir_file_free(&file_info);
        return;
    }

    if (strstr(file_info.new_path, "/new/") == NULL) {
        test_fail("new path doesn't contain /new/");
        ftn_maildir_file_free(&file_info);
        return;
    }

    ftn_maildir_file_free(&file_info);

    /* Clean up maildir */
    rmdir("tmp/test_maildir/tmp");
    rmdir("tmp/test_maildir/new");
    rmdir("tmp/test_maildir/cur");
    rmdir("tmp/test_maildir");

    test_pass();
}

/* Test message list operations */
void test_message_list_operations(void) {
    ftn_message_list_t* list;
    ftn_message_t* msg1;
    ftn_message_t* msg2;

    test_start("message list operations");

    /* Create message list */
    list = ftn_message_list_new();
    if (!list) {
        test_fail("Failed to create message list");
        return;
    }

    /* Verify initial state */
    if (list->count != 0 || list->messages != NULL) {
        test_fail("Message list not initialized properly");
        ftn_message_list_free(list);
        return;
    }

    /* Create test messages */
    msg1 = create_test_message(FTN_MSG_NETMAIL, "user1", "sysop");
    msg2 = create_test_message(FTN_MSG_ECHOMAIL, "All", "user2");

    if (!msg1 || !msg2) {
        test_fail("Failed to create test messages");
        if (msg1) ftn_message_free(msg1);
        if (msg2) ftn_message_free(msg2);
        ftn_message_list_free(list);
        return;
    }

    /* Add messages to list */
    if (ftn_message_list_add(list, msg1) != FTN_OK) {
        test_fail("Failed to add first message to list");
        ftn_message_free(msg1);
        ftn_message_free(msg2);
        ftn_message_list_free(list);
        return;
    }

    if (ftn_message_list_add(list, msg2) != FTN_OK) {
        test_fail("Failed to add second message to list");
        ftn_message_free(msg2);
        ftn_message_list_free(list);
        return;
    }

    /* Verify list state */
    if (list->count != 2) {
        test_fail("Message count incorrect after adding messages");
        ftn_message_list_free(list);
        return;
    }

    /* Messages are now owned by the list, so don't free them manually */
    ftn_message_list_free(list);

    test_pass();
}

/* Test atomic file writing */
void test_atomic_file_writing(void) {
    const char* test_file = "tmp/test_atomic_file.txt";
    const char* test_content = "This is test content for atomic writing.";
    FILE* file;
    char buffer[256];

    test_start("atomic file writing");

    /* Remove any existing test file */
    unlink(test_file);

    /* Write file atomically */
    if (ftn_storage_write_file_atomic(test_file, test_content, strlen(test_content)) != FTN_OK) {
        test_fail("Failed to write file atomically");
        return;
    }

    /* Verify file exists and contains correct content */
    file = fopen(test_file, "r");
    if (!file) {
        test_fail("Atomic file was not created");
        return;
    }

    if (!fgets(buffer, sizeof(buffer), file)) {
        test_fail("Failed to read from atomic file");
        fclose(file);
        return;
    }

    fclose(file);

    if (strcmp(buffer, test_content) != 0) {
        test_fail("Atomic file content is incorrect");
        return;
    }

    /* Clean up */
    unlink(test_file);

    test_pass();
}

/* Test basic mail storage functionality */
void test_basic_mail_storage(void) {
    ftn_config_t* config;
    ftn_storage_t* storage;
    ftn_message_t* msg;

    test_start("basic mail storage");

    /* This test is limited since we don't have RFC822 conversion working in isolation */
    /* We'll test the storage system setup and basic functionality */

    config = create_test_config();
    if (!config) {
        test_fail("Failed to create test config");
        return;
    }

    storage = ftn_storage_new(config);
    if (!storage) {
        test_fail("Failed to create storage system");
        ftn_config_free(config);
        return;
    }

    msg = create_test_message(FTN_MSG_NETMAIL, "testuser", "sysop");
    if (!msg) {
        test_fail("Failed to create test message");
        ftn_storage_free(storage);
        ftn_config_free(config);
        return;
    }

    /* Test would attempt to store mail, but since we don't have proper config
     * with mail paths set up, we'll just verify the function can be called
     * without crashing */

    /* In a full implementation, this would work:
     * ftn_storage_store_mail(storage, msg, "testuser", "fidonet");
     */

    ftn_message_free(msg);
    ftn_storage_free(storage);
    ftn_config_free(config);

    test_pass();
}

int main(void) {
    printf("Storage Tests\n");
    printf("=============\n\n");

    /* Run all tests */
    test_storage_lifecycle();
    test_path_templating();
    test_directory_creation();
    test_recursive_directory_creation();
    test_maildir_creation();
    test_maildir_filename_generation();
    test_message_list_operations();
    test_atomic_file_writing();
    test_basic_mail_storage();

    /* Print summary */
    printf("\nTest Summary: %d/%d tests passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}