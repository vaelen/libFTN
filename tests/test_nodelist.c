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

static void test_inet_protocol_functions(void) {
    printf("Testing Internet protocol functions...\n");
    
    /* Test protocol to string conversion */
    assert(strcmp(ftn_inet_protocol_to_string(FTN_INET_IBN), "Binkp") == 0);
    assert(strcmp(ftn_inet_protocol_to_string(FTN_INET_IFC), "ifcico") == 0);
    assert(strcmp(ftn_inet_protocol_to_string(FTN_INET_IFT), "FTP") == 0);
    assert(strcmp(ftn_inet_protocol_to_string(FTN_INET_ITN), "Telnet") == 0);
    assert(strcmp(ftn_inet_protocol_to_string(FTN_INET_IVM), "Vmodem") == 0);
    
    /* Test default ports */
    assert(ftn_inet_protocol_default_port(FTN_INET_IBN) == 24554);
    assert(ftn_inet_protocol_default_port(FTN_INET_IFC) == 60179);
    assert(ftn_inet_protocol_default_port(FTN_INET_IFT) == 21);
    assert(ftn_inet_protocol_default_port(FTN_INET_ITN) == 23);
    assert(ftn_inet_protocol_default_port(FTN_INET_IVM) == 3141);
    
    printf("Internet protocol functions: PASSED\n");
}

static void test_inet_flag_parsing(void) {
    ftn_inet_service_t* services;
    size_t count;
    
    printf("Testing Internet flag parsing...\n");
    
    /* Test basic protocol flag */
    count = ftn_nodelist_parse_inet_flags("IBN", &services);
    assert(count == 1);
    assert(services[0].protocol == FTN_INET_IBN);
    assert(services[0].hostname == NULL);
    assert(services[0].port == 24554);
    assert(services[0].has_port == 0);
    ftn_nodelist_free_inet_services(services, count);
    
    /* Test protocol with hostname */
    count = ftn_nodelist_parse_inet_flags("IBN:fido.example.com", &services);
    assert(count == 1);
    assert(services[0].protocol == FTN_INET_IBN);
    assert(strcmp(services[0].hostname, "fido.example.com") == 0);
    assert(services[0].port == 24554);
    assert(services[0].has_port == 0);
    ftn_nodelist_free_inet_services(services, count);
    
    /* Test protocol with hostname and port */
    count = ftn_nodelist_parse_inet_flags("IBN:fido.example.com:12345", &services);
    assert(count == 1);
    assert(services[0].protocol == FTN_INET_IBN);
    assert(strcmp(services[0].hostname, "fido.example.com") == 0);
    assert(services[0].port == 12345);
    assert(services[0].has_port == 1);
    ftn_nodelist_free_inet_services(services, count);
    
    /* Test protocol with port only */
    count = ftn_nodelist_parse_inet_flags("IBN:12345", &services);
    assert(count == 1);
    assert(services[0].protocol == FTN_INET_IBN);
    assert(services[0].hostname == NULL);
    assert(services[0].port == 12345);
    assert(services[0].has_port == 1);
    ftn_nodelist_free_inet_services(services, count);
    
    /* Test multiple protocols */
    count = ftn_nodelist_parse_inet_flags("IBN:fido.test.org,ITN:telnet.test.org:2323,IFT:21", &services);
    assert(count == 3);
    
    assert(services[0].protocol == FTN_INET_IBN);
    assert(strcmp(services[0].hostname, "fido.test.org") == 0);
    assert(services[0].port == 24554);
    
    assert(services[1].protocol == FTN_INET_ITN);
    assert(strcmp(services[1].hostname, "telnet.test.org") == 0);
    assert(services[1].port == 2323);
    
    assert(services[2].protocol == FTN_INET_IFT);
    assert(services[2].hostname == NULL);
    assert(services[2].port == 21);
    
    ftn_nodelist_free_inet_services(services, count);
    
    /* Test INA default hostname */
    count = ftn_nodelist_parse_inet_flags("INA:default.test.org,IBN,ITN:2323", &services);
    assert(count == 2);
    
    assert(services[0].protocol == FTN_INET_IBN);
    assert(strcmp(services[0].hostname, "default.test.org") == 0);
    assert(services[0].port == 24554);
    
    assert(services[1].protocol == FTN_INET_ITN);
    assert(strcmp(services[1].hostname, "default.test.org") == 0);
    assert(services[1].port == 2323);
    
    ftn_nodelist_free_inet_services(services, count);
    
    /* Test with non-Internet flags mixed in */
    count = ftn_nodelist_parse_inet_flags("CM,XA,IBN:bbs.test.org,V34,ITN:23", &services);
    assert(count == 2);
    
    assert(services[0].protocol == FTN_INET_IBN);
    assert(strcmp(services[0].hostname, "bbs.test.org") == 0);
    assert(services[0].port == 24554);
    
    assert(services[1].protocol == FTN_INET_ITN);
    assert(services[1].hostname == NULL);
    assert(services[1].port == 23);
    
    ftn_nodelist_free_inet_services(services, count);
    
    /* Test empty flags */
    count = ftn_nodelist_parse_inet_flags("", &services);
    assert(count == 0);
    
    /* Test NULL flags */
    count = ftn_nodelist_parse_inet_flags(NULL, &services);
    assert(count == 0);
    
    /* Test flags with no Internet protocols */
    count = ftn_nodelist_parse_inet_flags("CM,XA,V34,V42b", &services);
    assert(count == 0);
    
    printf("Internet flag parsing: PASSED\n");
}

static void test_inet_edge_cases(void) {
    ftn_inet_service_t* services;
    size_t count;
    
    printf("Testing Internet flag edge cases...\n");
    
    /* Test IVM protocol */
    count = ftn_nodelist_parse_inet_flags("IVM:vmodem.test.org:5555", &services);
    assert(count == 1);
    assert(services[0].protocol == FTN_INET_IVM);
    assert(strcmp(services[0].hostname, "vmodem.test.org") == 0);
    assert(services[0].port == 5555);
    ftn_nodelist_free_inet_services(services, count);
    
    /* Test IFC protocol */
    count = ftn_nodelist_parse_inet_flags("IFC:ifcico.test.org", &services);
    assert(count == 1);
    assert(services[0].protocol == FTN_INET_IFC);
    assert(strcmp(services[0].hostname, "ifcico.test.org") == 0);
    assert(services[0].port == 60179);
    ftn_nodelist_free_inet_services(services, count);
    
    /* Test whitespace handling */
    count = ftn_nodelist_parse_inet_flags(" IBN:test.org , ITN:23 ", &services);
    assert(count == 2);
    assert(services[0].protocol == FTN_INET_IBN);
    assert(strcmp(services[0].hostname, "test.org") == 0);
    assert(services[1].protocol == FTN_INET_ITN);
    assert(services[1].port == 23);
    ftn_nodelist_free_inet_services(services, count);
    
    /* Test INA with multiple protocols using default */
    count = ftn_nodelist_parse_inet_flags("INA:hub.test.org,IBN,IFT,ITN:2323", &services);
    assert(count == 3);
    
    assert(services[0].protocol == FTN_INET_IBN);
    assert(strcmp(services[0].hostname, "hub.test.org") == 0);
    assert(services[0].port == 24554);
    
    assert(services[1].protocol == FTN_INET_IFT);
    assert(strcmp(services[1].hostname, "hub.test.org") == 0);
    assert(services[1].port == 21);
    
    assert(services[2].protocol == FTN_INET_ITN);
    assert(strcmp(services[2].hostname, "hub.test.org") == 0);
    assert(services[2].port == 2323);
    
    ftn_nodelist_free_inet_services(services, count);
    
    printf("Internet flag edge cases: PASSED\n");
}

static void test_inet_flag_filtering(void) {
    char* filtered;
    
    printf("Testing Internet flag filtering...\n");
    
    /* Test basic filtering */
    filtered = ftn_nodelist_filter_inet_flags("CM,XA,IBN:test.org,V34");
    assert(filtered != NULL);
    assert(strcmp(filtered, "CM,XA,V34") == 0);
    free(filtered);
    
    /* Test filtering with multiple Internet flags */
    filtered = ftn_nodelist_filter_inet_flags("CM,XA,IBN:test.org,ITN:23,INA:default.org,V34,ICM");
    assert(filtered != NULL);
    assert(strcmp(filtered, "CM,XA,V34") == 0);
    free(filtered);
    
    /* Test filtering all Internet flags */
    filtered = ftn_nodelist_filter_inet_flags("IBN:test.org,ITN:23,INA:default.org,ICM");
    assert(filtered != NULL);
    assert(strcmp(filtered, "") == 0);
    free(filtered);
    
    /* Test filtering no Internet flags */
    filtered = ftn_nodelist_filter_inet_flags("CM,XA,V34,V42b");
    assert(filtered != NULL);
    assert(strcmp(filtered, "CM,XA,V34,V42b") == 0);
    free(filtered);
    
    /* Test empty flags */
    filtered = ftn_nodelist_filter_inet_flags("");
    assert(filtered != NULL);
    assert(strcmp(filtered, "") == 0);
    free(filtered);
    
    /* Test NULL flags */
    filtered = ftn_nodelist_filter_inet_flags(NULL);
    assert(filtered == NULL);
    
    /* Test filtering with INO4 flag */
    filtered = ftn_nodelist_filter_inet_flags("CM,XA,IBN:test.org,INO4,V34");
    assert(filtered != NULL);
    assert(strcmp(filtered, "CM,XA,V34") == 0);
    free(filtered);
    
    /* Test filtering with all protocol types */
    filtered = ftn_nodelist_filter_inet_flags("CM,IBN:test.org,IFC:raw.org,IFT:21,ITN:telnet.org,IVM:3333,XA");
    assert(filtered != NULL);
    assert(strcmp(filtered, "CM,XA") == 0);
    free(filtered);
    
    printf("Internet flag filtering: PASSED\n");
}

int main(void) {
    printf("Running nodelist tests...\n\n");
    
    test_address_functions();
    test_node_type_functions();
    test_line_parsing();
    test_comment_parsing();
    test_inet_protocol_functions();
    test_inet_flag_parsing();
    test_inet_edge_cases();
    test_inet_flag_filtering();
    
    printf("\nAll nodelist tests passed!\n");
    return 0;
}