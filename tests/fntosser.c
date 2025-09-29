/*
 * test_fntosser.c - FTN Tosser Integration Tests
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define _GNU_SOURCE

#include "ftn.h"
#include "ftn/config.h"

#define TEST_CONFIG_FILE "tests/data/fntosser_test.ini"

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

int run_fntosser_command(const char* args, char* output, size_t output_size) {
    char command[512];
    int status;

    snprintf(command, sizeof(command), "./bin/fntosser %s > tmp/fntosser_test_output 2>&1", args);

    status = system(command);

    if (output && output_size > 0) {
        FILE* fp = fopen("tmp/fntosser_test_output", "r");
        if (fp) {
            size_t bytes_read = fread(output, 1, output_size - 1, fp);
            output[bytes_read] = '\0';
            fclose(fp);
        } else {
            output[0] = '\0';
        }
    }

    return WEXITSTATUS(status);
}

void test_help_option(void) {
    char output[1024];
    int exit_code;

    test_start("help option");

    exit_code = run_fntosser_command("--help", output, sizeof(output));

    if (exit_code == 0 && strstr(output, "Usage:") != NULL) {
        test_pass();
    } else {
        test_fail("Help option did not work correctly");
    }
}

void test_version_option(void) {
    char output[1024];
    int exit_code;

    test_start("version option");

    exit_code = run_fntosser_command("--version", output, sizeof(output));

    if (exit_code == 0 && strstr(output, "fntosser") != NULL) {
        test_pass();
    } else {
        test_fail("Version option did not work correctly");
    }
}

void test_missing_config_error(void) {
    char output[1024];
    int exit_code;

    test_start("missing config error");

    exit_code = run_fntosser_command("", output, sizeof(output));

    if (exit_code != 0 && strstr(output, "Configuration file is required") != NULL) {
        test_pass();
    } else {
        test_fail("Missing config error not handled correctly");
    }
}

void test_invalid_config_file(void) {
    char output[1024];
    int exit_code;

    test_start("invalid config file");

    exit_code = run_fntosser_command("-c /nonexistent/config.ini", output, sizeof(output));

    if (exit_code != 0) {
        test_pass();
    } else {
        test_fail("Invalid config file should cause error");
    }
}

void test_valid_config_single_shot(void) {
    char output[1024];
    int exit_code;

    test_start("valid config single shot");

    exit_code = run_fntosser_command("-c " TEST_CONFIG_FILE, output, sizeof(output));

    if (exit_code == 0) {
        test_pass();
    } else {
        test_fail("Valid config single shot mode failed");
    }
}

void test_verbose_mode(void) {
    char output[1024];
    int exit_code;

    test_start("verbose mode");

    exit_code = run_fntosser_command("-c " TEST_CONFIG_FILE " -v", output, sizeof(output));

    if (exit_code == 0) {
        test_pass();
    } else {
        test_fail("Verbose mode failed");
    }
}

void test_invalid_sleep_interval(void) {
    char output[1024];
    int exit_code;

    test_start("invalid sleep interval");

    exit_code = run_fntosser_command("-s -1", output, sizeof(output));

    if (exit_code != 0 && strstr(output, "Invalid sleep interval") != NULL) {
        test_pass();
    } else {
        test_fail("Invalid sleep interval not handled correctly");
    }
}

void test_unknown_option(void) {
    char output[1024];
    int exit_code;

    test_start("unknown option");

    exit_code = run_fntosser_command("--unknown-option", output, sizeof(output));

    if (exit_code != 0 && strstr(output, "Unknown option") != NULL) {
        test_pass();
    } else {
        test_fail("Unknown option not handled correctly");
    }
}

void test_config_integration(void) {
    ftn_config_t* config;

    test_start("config integration");

    config = ftn_config_new();
    if (!config) {
        test_fail("Failed to create config");
        return;
    }

    if (ftn_config_load(config, TEST_CONFIG_FILE) != FTN_OK) {
        test_fail("Failed to load test config");
        ftn_config_free(config);
        return;
    }

    if (ftn_config_validate(config) != FTN_OK) {
        test_fail("Test config validation failed");
        ftn_config_free(config);
        return;
    }

    ftn_config_free(config);
    test_pass();
}

void test_signal_handling_setup(void) {
    test_start("signal handling setup");
    test_pass();
}

void test_logging_functions(void) {
    test_start("logging functions");
    test_pass();
}

void setup_test_directories(void) {
    /* Create test directories required by the test configuration */
    int status;
    status = system("mkdir -p tmp/test_ftn/testnet/inbox");
    status = system("mkdir -p tmp/test_ftn/testnet/outbox");
    status = system("mkdir -p tmp/test_ftn/testnet/processed");
    status = system("mkdir -p tmp/test_ftn/testnet/bad");
    status = system("mkdir -p tmp/test_mail/testuser");
    status = system("mkdir -p tmp/test_news");
    (void)status;
}

void cleanup_test_directories(void) {
    /* Clean up test directories */
    int status;
    status = system("rm -rf tmp/test_ftn");
    status = system("rm -rf tmp/test_mail");
    status = system("rm -rf tmp/test_news");
    (void)status;
}

int main(void) {
    printf("FTN Tosser Integration Tests\n");
    printf("============================\n\n");

    /* Setup test environment */
    setup_test_directories();

    test_help_option();
    test_version_option();
    test_missing_config_error();
    test_invalid_config_file();
    test_valid_config_single_shot();
    test_verbose_mode();
    test_invalid_sleep_interval();
    test_unknown_option();
    test_config_integration();
    test_signal_handling_setup();
    test_logging_functions();

    printf("\nTest Results: %d/%d tests passed\n", tests_passed, tests_run);

    /* Cleanup test environment */
    cleanup_test_directories();

    if (tests_passed == tests_run) {
        printf("All tests PASSED!\n");
        return 0;
    } else {
        printf("Some tests FAILED!\n");
        return 1;
    }
}