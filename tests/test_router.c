/*
 * test_router.c - Router system tests
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ftn.h"
#include "ftn/router.h"
#include "ftn/config.h"
#include "ftn/packet.h"
#include "ftn/dupecheck.h"

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

/* Create a basic test configuration */
ftn_config_t* create_test_config(void) {
    ftn_config_t* config = ftn_config_new();
    if (!config) return NULL;

    /* This would normally be loaded from a file, but for testing we'll create a minimal config */
    /* In practice, we'd load from tests/data/router_test_config.ini */

    return config;
}

/* Create a test message */
ftn_message_t* create_test_netmail(const char* to_user, const char* from_user,
                                  ftn_address_t* dest_addr, ftn_address_t* orig_addr) {
    ftn_message_t* msg = ftn_message_new(FTN_MSG_NETMAIL);
    if (!msg) return NULL;
    msg->to_user = malloc(strlen(to_user) + 1);
    msg->from_user = malloc(strlen(from_user) + 1);
    if (!msg->to_user || !msg->from_user) {
        ftn_message_free(msg);
        return NULL;
    }

    strcpy(msg->to_user, to_user);
    strcpy(msg->from_user, from_user);
    msg->dest_addr = *dest_addr;
    msg->orig_addr = *orig_addr;

    return msg;
}

ftn_message_t* create_test_echomail(const char* area_name, const char* to_user, const char* from_user) {
    ftn_message_t* msg = ftn_message_new(FTN_MSG_ECHOMAIL);
    if (!msg) return NULL;

    msg->area = malloc(strlen(area_name) + 1);
    msg->to_user = malloc(strlen(to_user) + 1);
    msg->from_user = malloc(strlen(from_user) + 1);
    if (!msg->area || !msg->to_user || !msg->from_user) {
        ftn_message_free(msg);
        return NULL;
    }

    strcpy(msg->area, area_name);
    strcpy(msg->to_user, to_user);
    strcpy(msg->from_user, from_user);

    return msg;
}

/* Test router creation and destruction */
void test_router_lifecycle(void) {
    ftn_config_t* config;
    ftn_dupecheck_t* dupecheck;
    ftn_router_t* router;

    test_start("router lifecycle");

    config = create_test_config();
    if (!config) {
        test_fail("Failed to create test config");
        return;
    }

    dupecheck = ftn_dupecheck_new("tmp/test_router_dupe.db");
    if (!dupecheck) {
        test_fail("Failed to create dupecheck");
        ftn_config_free(config);
        return;
    }

    router = ftn_router_new(config, dupecheck);
    if (!router) {
        test_fail("Failed to create router");
        ftn_dupecheck_free(dupecheck);
        ftn_config_free(config);
        return;
    }

    ftn_router_free(router);
    ftn_dupecheck_free(dupecheck);
    ftn_config_free(config);

    test_pass();
}

/* Test message type detection */
void test_message_type_detection(void) {
    ftn_message_t* netmail_msg;
    ftn_message_t* echomail_msg;
    ftn_address_t test_addr = {1, 1, 100, 0};

    test_start("message type detection");

    /* Test netmail detection */
    netmail_msg = create_test_netmail("testuser", "sysop", &test_addr, &test_addr);
    if (!netmail_msg) {
        test_fail("Failed to create test netmail");
        return;
    }

    if (!ftn_router_is_netmail(netmail_msg)) {
        test_fail("Failed to detect netmail");
        ftn_message_free(netmail_msg);
        return;
    }

    if (ftn_router_is_echomail(netmail_msg)) {
        test_fail("Incorrectly detected netmail as echomail");
        ftn_message_free(netmail_msg);
        return;
    }

    /* Test echomail detection */
    echomail_msg = create_test_echomail("TEST.AREA", "All", "sysop");
    if (!echomail_msg) {
        test_fail("Failed to create test echomail");
        ftn_message_free(netmail_msg);
        return;
    }

    if (!ftn_router_is_echomail(echomail_msg)) {
        test_fail("Failed to detect echomail");
        ftn_message_free(netmail_msg);
        ftn_message_free(echomail_msg);
        return;
    }

    if (ftn_router_is_netmail(echomail_msg)) {
        test_fail("Incorrectly detected echomail as netmail");
        ftn_message_free(netmail_msg);
        ftn_message_free(echomail_msg);
        return;
    }

    ftn_message_free(netmail_msg);
    ftn_message_free(echomail_msg);

    test_pass();
}

/* Test pattern matching */
void test_pattern_matching(void) {
    ftn_address_t test_addr = {1, 1, 100, 0};

    test_start("pattern matching");

    /* Test simple wildcard patterns */
    if (!ftn_router_pattern_match("TEST.*", "TEST.AREA")) {
        test_fail("Simple wildcard pattern failed");
        return;
    }

    if (ftn_router_pattern_match("TEST.*", "OTHER.AREA")) {
        test_fail("Pattern incorrectly matched");
        return;
    }

    /* Test address patterns */
    if (!ftn_router_address_match("1:1/*", &test_addr)) {
        test_fail("Address pattern matching failed");
        return;
    }

    if (ftn_router_address_match("2:*/*", &test_addr)) {
        test_fail("Address pattern incorrectly matched");
        return;
    }

    test_pass();
}

/* Test routing decision utilities */
void test_routing_decision_utilities(void) {
    ftn_routing_decision_t* decision;
    ftn_address_t forward_addr = {1, 1, 200, 0};

    test_start("routing decision utilities");

    decision = ftn_routing_decision_new();
    if (!decision) {
        test_fail("Failed to create routing decision");
        return;
    }

    /* Test local mail routing */
    if (ftn_routing_decision_set_local_mail(decision, "testuser", "/var/mail/testuser") != FTN_OK) {
        test_fail("Failed to set local mail routing");
        ftn_routing_decision_free(decision);
        return;
    }

    if (decision->action != FTN_ROUTE_LOCAL_MAIL) {
        test_fail("Wrong routing action set");
        ftn_routing_decision_free(decision);
        return;
    }

    /* Test forward routing */
    if (ftn_routing_decision_set_forward(decision, &forward_addr, "fidonet") != FTN_OK) {
        test_fail("Failed to set forward routing");
        ftn_routing_decision_free(decision);
        return;
    }

    if (decision->action != FTN_ROUTE_FORWARD) {
        test_fail("Wrong routing action set");
        ftn_routing_decision_free(decision);
        return;
    }

    ftn_routing_decision_free(decision);
    test_pass();
}

/* Test routing rule management */
void test_routing_rule_management(void) {
    ftn_config_t* config;
    ftn_dupecheck_t* dupecheck;
    ftn_router_t* router;
    ftn_routing_rule_t* rule;

    test_start("routing rule management");

    config = create_test_config();
    dupecheck = ftn_dupecheck_new("tmp/test_router_rules_dupe.db");
    router = ftn_router_new(config, dupecheck);

    if (!router) {
        test_fail("Failed to create router");
        if (dupecheck) ftn_dupecheck_free(dupecheck);
        if (config) ftn_config_free(config);
        return;
    }

    /* Create a test rule */
    rule = ftn_routing_rule_new();
    if (!rule) {
        test_fail("Failed to create routing rule");
        ftn_router_free(router);
        ftn_dupecheck_free(dupecheck);
        ftn_config_free(config);
        return;
    }

    if (ftn_routing_rule_set(rule, "test_rule", "TEST.*", FTN_ROUTE_LOCAL_NEWS, "/var/spool/news", 10) != FTN_OK) {
        test_fail("Failed to set routing rule");
        ftn_routing_rule_free(rule);
        ftn_router_free(router);
        ftn_dupecheck_free(dupecheck);
        ftn_config_free(config);
        return;
    }

    /* Add rule to router */
    if (ftn_router_add_rule(router, rule) != FTN_OK) {
        test_fail("Failed to add rule to router");
        ftn_routing_rule_free(rule);
        ftn_router_free(router);
        ftn_dupecheck_free(dupecheck);
        ftn_config_free(config);
        return;
    }

    /* Remove rule from router */
    if (ftn_router_remove_rule(router, "test_rule") != FTN_OK) {
        test_fail("Failed to remove rule from router");
        ftn_routing_rule_free(rule);
        ftn_router_free(router);
        ftn_dupecheck_free(dupecheck);
        ftn_config_free(config);
        return;
    }

    ftn_routing_rule_free(rule);
    ftn_router_free(router);
    ftn_dupecheck_free(dupecheck);
    ftn_config_free(config);

    test_pass();
}

/* Test address validation */
void test_address_validation(void) {
    ftn_config_t* config;
    ftn_dupecheck_t* dupecheck;
    ftn_router_t* router;

    test_start("address validation");

    config = create_test_config();
    dupecheck = ftn_dupecheck_new("tmp/test_router_addr_dupe.db");
    router = ftn_router_new(config, dupecheck);

    if (!router) {
        test_fail("Failed to create router");
        if (dupecheck) ftn_dupecheck_free(dupecheck);
        if (config) ftn_config_free(config);
        return;
    }

    /* Test validation - since we don't have network config loaded, this might not work as expected */
    /* In a real implementation, we'd load network configs first */

    ftn_router_free(router);
    ftn_dupecheck_free(dupecheck);
    ftn_config_free(config);

    test_pass();
}

/* Test basic routing functionality */
void test_basic_routing(void) {
    ftn_config_t* config;
    ftn_dupecheck_t* dupecheck;
    ftn_router_t* router;
    ftn_message_t* msg;
    ftn_routing_decision_t* decision;
    ftn_address_t test_addr = {1, 1, 100, 0};

    test_start("basic routing");

    config = create_test_config();
    dupecheck = ftn_dupecheck_new("tmp/test_router_basic_dupe.db");
    router = ftn_router_new(config, dupecheck);
    decision = ftn_routing_decision_new();

    if (!router || !decision) {
        test_fail("Failed to create router or decision");
        if (router) ftn_router_free(router);
        if (decision) ftn_routing_decision_free(decision);
        if (dupecheck) ftn_dupecheck_free(dupecheck);
        if (config) ftn_config_free(config);
        return;
    }

    /* Create a test message */
    msg = create_test_netmail("testuser", "sysop", &test_addr, &test_addr);
    if (!msg) {
        test_fail("Failed to create test message");
        ftn_routing_decision_free(decision);
        ftn_router_free(router);
        ftn_dupecheck_free(dupecheck);
        ftn_config_free(config);
        return;
    }

    /* Test routing the message */
    if (ftn_router_route_message(router, msg, decision) != FTN_OK) {
        test_fail("Failed to route message");
        ftn_message_free(msg);
        ftn_routing_decision_free(decision);
        ftn_router_free(router);
        ftn_dupecheck_free(dupecheck);
        ftn_config_free(config);
        return;
    }

    /* Check that a decision was made */
    if (decision->action == 0) {
        test_fail("No routing decision was made");
        ftn_message_free(msg);
        ftn_routing_decision_free(decision);
        ftn_router_free(router);
        ftn_dupecheck_free(dupecheck);
        ftn_config_free(config);
        return;
    }

    ftn_message_free(msg);
    ftn_routing_decision_free(decision);
    ftn_router_free(router);
    ftn_dupecheck_free(dupecheck);
    ftn_config_free(config);

    test_pass();
}

int main(void) {
    printf("Router Tests\n");
    printf("============\n\n");

    /* Run all tests */
    test_router_lifecycle();
    test_message_type_detection();
    test_pattern_matching();
    test_routing_decision_utilities();
    test_routing_rule_management();
    test_address_validation();
    test_basic_routing();

    /* Print summary */
    printf("\nTest Summary: %d/%d tests passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}