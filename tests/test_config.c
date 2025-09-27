/*
 * test_config.c - Configuration system tests
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ftn.h"
#include "ftn/config.h"

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

/* Test INI parsing functions */
void test_ini_basic_parsing(void) {
    ftn_config_ini_t* ini;
    const char* value;

    test_start("INI basic parsing");

    ini = ftn_config_ini_new();
    assert(ini != NULL);

    if (ftn_config_ini_parse(ini, "tests/data/valid_config.ini") != FTN_OK) {
        test_fail("Failed to parse valid config");
        ftn_config_ini_free(ini);
        return;
    }

    /* Test case-insensitive section lookup */
    if (!ftn_config_ini_has_section(ini, "node")) {
        test_fail("Missing node section");
        ftn_config_ini_free(ini);
        return;
    }

    if (!ftn_config_ini_has_section(ini, "NODE")) {
        test_fail("Case insensitive section lookup failed");
        ftn_config_ini_free(ini);
        return;
    }

    /* Test case-insensitive key lookup */
    value = ftn_config_ini_get_value(ini, "node", "name");
    if (!value || strcmp(value, "Test Node BBS") != 0) {
        test_fail("Failed to get node name");
        ftn_config_ini_free(ini);
        return;
    }

    value = ftn_config_ini_get_value(ini, "NODE", "NAME");
    if (!value || strcmp(value, "Test Node BBS") != 0) {
        test_fail("Case insensitive key lookup failed");
        ftn_config_ini_free(ini);
        return;
    }

    ftn_config_ini_free(ini);
    test_pass();
}

void test_ini_comments_whitespace(void) {
    ftn_config_ini_t* ini;

    test_start("INI comments and whitespace");

    ini = ftn_config_ini_new();
    assert(ini != NULL);

    if (ftn_config_ini_parse(ini, "tests/data/valid_config.ini") != FTN_OK) {
        test_fail("Failed to parse config with comments");
        ftn_config_ini_free(ini);
        return;
    }

    /* Comments should be ignored, only real sections should exist */
    if (!ftn_config_ini_has_section(ini, "node")) {
        test_fail("Missing node section");
        ftn_config_ini_free(ini);
        return;
    }

    ftn_config_ini_free(ini);
    test_pass();
}

void test_ini_invalid_syntax(void) {
    ftn_config_ini_t* ini;

    test_start("INI invalid syntax handling");

    ini = ftn_config_ini_new();
    assert(ini != NULL);

    if (ftn_config_ini_parse(ini, "tests/data/invalid_syntax.ini") == FTN_OK) {
        test_fail("Should have failed to parse invalid syntax");
        ftn_config_ini_free(ini);
        return;
    }

    ftn_config_ini_free(ini);
    test_pass();
}

/* Test path templating */
void test_path_templating_user(void) {
    char* result;

    test_start("path templating with %USER%");

    result = ftn_config_expand_path("/var/mail/%USER%", "testuser", NULL);
    if (!result || strcmp(result, "/var/mail/testuser") != 0) {
        test_fail("USER substitution failed");
        if (result) free(result);
        return;
    }

    free(result);
    test_pass();
}

void test_path_templating_network(void) {
    char* result;

    test_start("path templating with %NETWORK%");

    result = ftn_config_expand_path("/var/spool/%NETWORK%", NULL, "fidonet");
    if (!result || strcmp(result, "/var/spool/fidonet") != 0) {
        test_fail("NETWORK substitution failed");
        if (result) free(result);
        return;
    }

    free(result);
    test_pass();
}

void test_path_templating_combined(void) {
    char* result;

    test_start("path templating with both %USER% and %NETWORK%");

    result = ftn_config_expand_path("/var/spool/%NETWORK%/%USER%", "testuser", "fidonet");
    if (!result || strcmp(result, "/var/spool/fidonet/testuser") != 0) {
        test_fail("Combined substitution failed");
        if (result) free(result);
        return;
    }

    free(result);
    test_pass();
}

void test_path_templating_no_substitution(void) {
    char* result;

    test_start("path templating with no substitution needed");

    result = ftn_config_expand_path("/var/spool/static", "testuser", "fidonet");
    if (!result || strcmp(result, "/var/spool/static") != 0) {
        test_fail("Static path failed");
        if (result) free(result);
        return;
    }

    free(result);
    test_pass();
}

/* Test configuration loading */
void test_config_load_valid(void) {
    ftn_config_t* config;

    test_start("configuration loading valid file");

    config = ftn_config_new();
    assert(config != NULL);

    if (ftn_config_load(config, "tests/data/valid_config.ini") != FTN_OK) {
        test_fail("Failed to load valid config");
        ftn_config_free(config);
        return;
    }

    if (!config->node || !config->node->name) {
        test_fail("Missing node configuration");
        ftn_config_free(config);
        return;
    }

    if (strcmp(config->node->name, "Test Node BBS") != 0) {
        test_fail("Wrong node name");
        ftn_config_free(config);
        return;
    }

    if (config->network_count != 2) {
        test_fail("Wrong number of networks");
        ftn_config_free(config);
        return;
    }

    ftn_config_free(config);
    test_pass();
}

void test_config_validation_valid(void) {
    ftn_config_t* config;

    test_start("configuration validation of valid config");

    config = ftn_config_new();
    assert(config != NULL);

    if (ftn_config_load(config, "tests/data/valid_config.ini") != FTN_OK) {
        test_fail("Failed to load config");
        ftn_config_free(config);
        return;
    }

    if (ftn_config_validate(config) != FTN_OK) {
        test_fail("Valid config failed validation");
        ftn_config_free(config);
        return;
    }

    ftn_config_free(config);
    test_pass();
}

void test_config_validation_missing_sections(void) {
    ftn_config_t* config;

    test_start("configuration validation missing required sections");

    config = ftn_config_new();
    assert(config != NULL);

    if (ftn_config_load(config, "tests/data/missing_sections.ini") == FTN_OK) {
        if (ftn_config_validate(config) == FTN_OK) {
            test_fail("Should have failed validation");
            ftn_config_free(config);
            return;
        }
    }

    ftn_config_free(config);
    test_pass();
}

/* Test multi-network support */
void test_multi_network_support(void) {
    ftn_config_t* config;
    const ftn_network_config_t* net;

    test_start("multi-network configuration support");

    config = ftn_config_new();
    assert(config != NULL);

    if (ftn_config_load(config, "tests/data/multi_network.ini") != FTN_OK) {
        test_fail("Failed to load multi-network config");
        ftn_config_free(config);
        return;
    }

    if (config->network_count != 3) {
        test_fail("Wrong number of networks");
        ftn_config_free(config);
        return;
    }

    /* Test network lookup by name */
    net = ftn_config_get_network(config, "fidonet");
    if (!net || strcmp(net->name, "Fidonet") != 0) {
        test_fail("Failed to find fidonet network");
        ftn_config_free(config);
        return;
    }

    net = ftn_config_get_network(config, "fsxnet");
    if (!net || strcmp(net->name, "fsxNet") != 0) {
        test_fail("Failed to find fsxnet network");
        ftn_config_free(config);
        return;
    }

    net = ftn_config_get_network(config, "micronet");
    if (!net || strcmp(net->name, "MicroNet") != 0) {
        test_fail("Failed to find micronet network");
        ftn_config_free(config);
        return;
    }

    ftn_config_free(config);
    test_pass();
}

void test_case_insensitive_parsing(void) {
    ftn_config_t* config;

    test_start("case insensitive INI parsing");

    config = ftn_config_new();
    assert(config != NULL);

    if (ftn_config_load(config, "tests/data/case_insensitive.ini") != FTN_OK) {
        test_fail("Failed to load case insensitive config");
        ftn_config_free(config);
        return;
    }

    if (!config->node || !config->node->name) {
        test_fail("Missing node configuration");
        ftn_config_free(config);
        return;
    }

    if (strcmp(config->node->name, "Test Node") != 0) {
        test_fail("Wrong node name from uppercase section");
        ftn_config_free(config);
        return;
    }

    if (config->network_count != 1) {
        test_fail("Wrong number of networks");
        ftn_config_free(config);
        return;
    }

    ftn_config_free(config);
    test_pass();
}

/* Test networks list parsing */
void test_networks_list_parsing(void) {
    char** networks;
    size_t count;

    test_start("networks list parsing");

    networks = (char**)ftn_config_parse_networks_list("fidonet,fsxnet,micronet", &count);
    if (!networks || count != 3) {
        test_fail("Failed to parse networks list");
        return;
    }

    if (strcmp(networks[0], "fidonet") != 0 ||
        strcmp(networks[1], "fsxnet") != 0 ||
        strcmp(networks[2], "micronet") != 0) {
        test_fail("Wrong network names");
        return;
    }

    /* Cleanup */
    if (networks) {
        size_t i;
        for (i = 0; i < count; i++) {
            if (networks[i]) free(networks[i]);
        }
        free(networks);
    }

    test_pass();
}

void test_networks_list_whitespace(void) {
    char** networks;
    size_t count;

    test_start("networks list with whitespace");

    networks = (char**)ftn_config_parse_networks_list(" fidonet , fsxnet , micronet ", &count);
    if (!networks || count != 3) {
        test_fail("Failed to parse networks list with whitespace");
        return;
    }

    if (strcmp(networks[0], "fidonet") != 0 ||
        strcmp(networks[1], "fsxnet") != 0 ||
        strcmp(networks[2], "micronet") != 0) {
        test_fail("Wrong network names after trimming");
        return;
    }

    /* Cleanup */
    if (networks) {
        size_t i;
        for (i = 0; i < count; i++) {
            if (networks[i]) free(networks[i]);
        }
        free(networks);
    }

    test_pass();
}

/* Test string utilities */
void test_string_utilities(void) {
    char test_str[100];
    char* dup_str;

    test_start("string utility functions");

    /* Test trimming */
    strcpy(test_str, "  test string  ");
    ftn_config_trim(test_str);
    if (strcmp(test_str, "test string") != 0) {
        test_fail("String trimming failed");
        return;
    }

    /* Test case insensitive comparison */
    if (ftn_config_strcasecmp("Test", "TEST") != 0) {
        test_fail("Case insensitive comparison failed");
        return;
    }

    if (ftn_config_strcasecmp("abc", "xyz") >= 0) {
        test_fail("Case insensitive comparison ordering failed");
        return;
    }

    /* Test string duplication */
    dup_str = ftn_config_strdup("test string");
    if (!dup_str || strcmp(dup_str, "test string") != 0) {
        test_fail("String duplication failed");
        if (dup_str) free(dup_str);
        return;
    }
    free(dup_str);

    test_pass();
}

/* Test error conditions */
void test_error_conditions(void) {
    ftn_config_t* config;
    char* result;

    test_start("error condition handling");

    /* Test invalid parameters */
    config = ftn_config_new();
    assert(config != NULL);

    if (ftn_config_load(config, NULL) == FTN_OK) {
        test_fail("Should fail with NULL filename");
        ftn_config_free(config);
        return;
    }

    if (ftn_config_load(NULL, "test.ini") == FTN_OK) {
        test_fail("Should fail with NULL config");
        ftn_config_free(config);
        return;
    }

    /* Test file not found */
    if (ftn_config_load(config, "/nonexistent/file.ini") == FTN_OK) {
        test_fail("Should fail with nonexistent file");
        ftn_config_free(config);
        return;
    }

    ftn_config_free(config);

    /* Test path templating with NULL */
    result = ftn_config_expand_path(NULL, "user", "network");
    if (result != NULL) {
        test_fail("Should return NULL for NULL template");
        free(result);
        return;
    }

    test_pass();
}

/* Test accessor functions */
void test_accessor_functions(void) {
    ftn_config_t* config;
    const ftn_node_config_t* node;
    const ftn_mail_config_t* mail;
    const ftn_news_config_t* news;
    const ftn_network_config_t* network;

    test_start("configuration accessor functions");

    config = ftn_config_new();
    assert(config != NULL);

    if (ftn_config_load(config, "tests/data/valid_config.ini") != FTN_OK) {
        test_fail("Failed to load config");
        ftn_config_free(config);
        return;
    }

    /* Test node accessor */
    node = ftn_config_get_node(config);
    if (!node || !node->name || strcmp(node->name, "Test Node BBS") != 0) {
        test_fail("Node accessor failed");
        ftn_config_free(config);
        return;
    }

    /* Test mail accessor */
    mail = ftn_config_get_mail(config);
    if (!mail || !mail->inbox) {
        test_fail("Mail accessor failed");
        ftn_config_free(config);
        return;
    }

    /* Test news accessor */
    news = ftn_config_get_news(config);
    if (!news || !news->path) {
        test_fail("News accessor failed");
        ftn_config_free(config);
        return;
    }

    /* Test network accessor */
    network = ftn_config_get_network(config, "fidonet");
    if (!network || !network->name || strcmp(network->name, "Fidonet") != 0) {
        test_fail("Network accessor failed");
        ftn_config_free(config);
        return;
    }

    /* Test non-existent network */
    network = ftn_config_get_network(config, "nonexistent");
    if (network != NULL) {
        test_fail("Should return NULL for non-existent network");
        ftn_config_free(config);
        return;
    }

    ftn_config_free(config);
    test_pass();
}

int main(void) {
    printf("Configuration System Tests\n");
    printf("==========================\n\n");

    /* INI parsing tests */
    test_ini_basic_parsing();
    test_ini_comments_whitespace();
    test_ini_invalid_syntax();

    /* Path templating tests */
    test_path_templating_user();
    test_path_templating_network();
    test_path_templating_combined();
    test_path_templating_no_substitution();

    /* Configuration loading tests */
    test_config_load_valid();
    test_config_validation_valid();
    test_config_validation_missing_sections();

    /* Multi-network tests */
    test_multi_network_support();
    test_case_insensitive_parsing();

    /* Networks list parsing tests */
    test_networks_list_parsing();
    test_networks_list_whitespace();

    /* String utility tests */
    test_string_utilities();

    /* Error condition tests */
    test_error_conditions();

    /* Accessor function tests */
    test_accessor_functions();

    printf("\nTest Results: %d/%d tests passed\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("All tests PASSED!\n");
        return 0;
    } else {
        printf("Some tests FAILED!\n");
        return 1;
    }
}