/*
 * test_nodelist - Nodelist Test Suite
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "../include/ftn.h"
#include <assert.h>

static void test_address_functions(void) {
    ftn_address_t addr1, addr2;
    char buffer[32];
    
    printf("Testing address functions...\n");
    
    /* Test address parsing */
    assert(ftn_address_parse("21:1/100", &addr1));
    assert(addr1.zone == 21);
    assert(addr1.net == 1);
    assert(addr1.node == 100);
    assert(addr1.point == 0);
    
    assert(ftn_address_parse("21:1/100.5", &addr2));
    assert(addr2.zone == 21);
    assert(addr2.net == 1);
    assert(addr2.node == 100);
    assert(addr2.point == 5);
    
    /* Test address comparison */
    assert(ftn_address_compare(&addr1, &addr1) == 0);
    assert(ftn_address_compare(&addr1, &addr2) != 0);
    
    /* Test address to string */
    ftn_address_to_string(&addr1, buffer, sizeof(buffer));
    assert(strcmp(buffer, "21:1/100") == 0);
    
    ftn_address_to_string(&addr2, buffer, sizeof(buffer));
    assert(strcmp(buffer, "21:1/100.5") == 0);
    
    printf("Address functions: PASSED\n");
}

static void test_node_type_functions(void) {
    printf("Testing node type functions...\n");
    
    assert(ftn_node_type_from_string("Zone") == FTN_NODE_ZONE);
    assert(ftn_node_type_from_string("Region") == FTN_NODE_REGION);
    assert(ftn_node_type_from_string("Host") == FTN_NODE_HOST);
    assert(ftn_node_type_from_string("Hub") == FTN_NODE_HUB);
    assert(ftn_node_type_from_string("Pvt") == FTN_NODE_PVT);
    assert(ftn_node_type_from_string("Hold") == FTN_NODE_HOLD);
    assert(ftn_node_type_from_string("Down") == FTN_NODE_DOWN);
    assert(ftn_node_type_from_string("") == FTN_NODE_NORMAL);
    assert(ftn_node_type_from_string(NULL) == FTN_NODE_NORMAL);
    
    assert(strcmp(ftn_node_type_to_string(FTN_NODE_ZONE), "Zone") == 0);
    assert(strcmp(ftn_node_type_to_string(FTN_NODE_REGION), "Region") == 0);
    assert(strcmp(ftn_node_type_to_string(FTN_NODE_HOST), "Host") == 0);
    assert(strcmp(ftn_node_type_to_string(FTN_NODE_HUB), "Hub") == 0);
    assert(strcmp(ftn_node_type_to_string(FTN_NODE_PVT), "Pvt") == 0);
    assert(strcmp(ftn_node_type_to_string(FTN_NODE_HOLD), "Hold") == 0);
    assert(strcmp(ftn_node_type_to_string(FTN_NODE_DOWN), "Down") == 0);
    assert(strcmp(ftn_node_type_to_string(FTN_NODE_NORMAL), "Node") == 0);
    
    printf("Node type functions: PASSED\n");
}

static void test_line_parsing(void) {
    ftn_nodelist_entry_t* entry;
    ftn_error_t result;
    
    printf("Testing line parsing...\n");
    
    entry = ftn_nodelist_entry_new();
    assert(entry != NULL);
    
    /* Test normal node line */
    result = ftn_nodelist_parse_line(",101,Agency_BBS,Dunedin_NZL,Paul_Hayton,-Unpublished-,300,CM", entry);
    assert(result == FTN_OK);
    assert(entry->type == FTN_NODE_NORMAL);
    assert(entry->address.node == 101);
    assert(strcmp(entry->name, "Agency BBS") == 0);
    assert(strcmp(entry->location, "Dunedin NZL") == 0);
    assert(strcmp(entry->sysop, "Paul Hayton") == 0);
    assert(strcmp(entry->phone, "-Unpublished-") == 0);
    assert(strcmp(entry->speed, "300") == 0);
    assert(strcmp(entry->flags, "CM") == 0);
    
    ftn_nodelist_entry_free(entry);
    
    entry = ftn_nodelist_entry_new();
    assert(entry != NULL);
    
    /* Test zone line */
    result = ftn_nodelist_parse_line("Zone,21,fsxNet_ZC,Dunedin_NZL,Paul_Hayton,-Unpublished-,300,ICM", entry);
    assert(result == FTN_OK);
    assert(entry->type == FTN_NODE_ZONE);
    assert(entry->address.zone == 21);
    assert(entry->address.net == 0);
    assert(entry->address.node == 0);
    
    ftn_nodelist_entry_free(entry);
    
    printf("Line parsing: PASSED\n");
}

static void test_comment_parsing(void) {
    ftn_comment_flags_t flags;
    char* text;
    ftn_error_t result;
    
    printf("Testing comment parsing...\n");
    
    result = ftn_nodelist_parse_comment(";A fsxNet Nodelist for Friday, August 8, 2025", &flags, &text);
    assert(result == FTN_OK);
    assert(flags == FTN_COMMENT_ALL);
    assert(strcmp(text, "fsxNet Nodelist for Friday, August 8, 2025") == 0);
    free(text);
    
    result = ftn_nodelist_parse_comment(";S This is for sysops", &flags, &text);
    assert(result == FTN_OK);
    assert(flags == FTN_COMMENT_SYSOP);
    assert(strcmp(text, "This is for sysops") == 0);
    free(text);
    
    result = ftn_nodelist_parse_comment("; Just a comment", &flags, &text);
    assert(result == FTN_OK);
    assert(flags == FTN_COMMENT_NONE);
    assert(strcmp(text, "Just a comment") == 0);
    free(text);
    
    printf("Comment parsing: PASSED\n");
}

int main(void) {
    printf("Running nodelist tests...\n\n");
    
    test_address_functions();
    test_node_type_functions();
    test_line_parsing();
    test_comment_parsing();
    
    printf("\nAll nodelist tests passed!\n");
    return 0;
}