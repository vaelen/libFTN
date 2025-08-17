/*
 * nllookup - Nodelist Lookup Utility
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 * 
 * Example program to search for entries in a FidoNet nodelist
 */

#include <ftn.h>

static void print_version(void) {
    printf("nllookup (libFTN) %s\n", ftn_get_version());
    printf("%s\n", ftn_get_copyright());
    printf("License: %s\n", ftn_get_license());
}

static void print_usage(const char* progname) {
    printf("Usage: %s [options] <nodelist_file> <command> [args]\n", progname);
    printf("Options:\n");
    printf("  -h, --help     Show this help message\n");
    printf("      --version  Show version information\n");
    printf("\nCommands:\n");
    printf("  addr <zone:net/node>     - Find by FTN address\n");
    printf("  name <name>              - Find by BBS name\n");
    printf("  sysop <sysop>            - Find by sysop name\n");
    printf("  zones                    - List all zones\n");
    printf("  nets <zone>              - List nets in zone\n");
    printf("  nodes <zone> <net>       - List nodes in net\n");
}

static void print_entry(const ftn_nodelist_entry_t* entry) {
    char addr_str[32];
    ftn_inet_service_t* services;
    size_t service_count;
    size_t i;
    char* filtered_flags;
    
    ftn_address_to_string(&entry->address, addr_str, sizeof(addr_str));
    
    printf("Type:     %s\n", ftn_node_type_to_string(entry->type));
    printf("Address:  %s\n", addr_str);
    printf("Name:     %s\n", entry->name ? entry->name : "");
    printf("Location: %s\n", entry->location ? entry->location : "");
    printf("Sysop:    %s\n", entry->sysop ? entry->sysop : "");
    printf("Phone:    %s\n", entry->phone ? entry->phone : "");
    printf("Speed:    %s\n", entry->speed ? entry->speed : "");
    
    /* Display flags with Internet flags filtered out */
    filtered_flags = ftn_nodelist_filter_inet_flags(entry->flags);
    printf("Flags:    %s\n", filtered_flags ? filtered_flags : "");
    if (filtered_flags) free(filtered_flags);
    
    /* Parse and display Internet services */
    service_count = ftn_nodelist_parse_inet_flags(entry->flags, &services);
    if (service_count > 0) {
        printf("Internet Services:\n");
        for (i = 0; i < service_count; i++) {
            if (services[i].hostname) {
                printf("  %s: %s:%u\n", 
                       ftn_inet_protocol_to_string(services[i].protocol),
                       services[i].hostname,
                       services[i].port);
            } else {
                printf("  %s: (no hostname):%u\n",
                       ftn_inet_protocol_to_string(services[i].protocol),
                       services[i].port);
            }
        }
        ftn_nodelist_free_inet_services(services, service_count);
    }
    
    printf("\n");
}

int main(int argc, char* argv[]) {
    ftn_nodelist_t* nodelist;
    ftn_error_t result;
    ftn_nodelist_entry_t* entry;
    ftn_address_t addr;
    unsigned int* list;
    ftn_nodelist_entry_t** nodes;
    size_t count, i;
    
    /* Check for options first */
    if (argc >= 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[1], "--version") == 0) {
            print_version();
            return 0;
        }
    }
    
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    printf("Loading nodelist: %s\n", argv[1]);
    
    result = ftn_nodelist_load(argv[1], &nodelist);
    if (result != FTN_OK) {
        switch (result) {
            case FTN_ERROR_FILE:
                printf("Error: Cannot open file '%s'\n", argv[1]);
                break;
            case FTN_ERROR_NOMEM:
                printf("Error: Out of memory\n");
                break;
            case FTN_ERROR_CRC:
                printf("Error: CRC mismatch - file may be corrupted\n");
                break;
            default:
                printf("Error: Unknown error (%d)\n", result);
                break;
        }
        return 1;
    }
    
    printf("Loaded %lu entries\n\n", (unsigned long)nodelist->count);
    
    if (strcmp(argv[2], "addr") == 0) {
        if (argc != 4) {
            printf("Usage: %s %s addr <zone:net/node>\n", argv[0], argv[1]);
            ftn_nodelist_free(nodelist);
            return 1;
        }
        
        if (!ftn_address_parse(argv[3], &addr)) {
            printf("Error: Invalid address format '%s'\n", argv[3]);
            ftn_nodelist_free(nodelist);
            return 1;
        }
        
        entry = ftn_nodelist_find_by_address(nodelist, &addr);
        if (entry) {
            printf("Found entry:\n");
            print_entry(entry);
        } else {
            printf("Address not found: %s\n", argv[3]);
        }
    } 
    else if (strcmp(argv[2], "name") == 0) {
        if (argc != 4) {
            printf("Usage: %s %s name <name>\n", argv[0], argv[1]);
            ftn_nodelist_free(nodelist);
            return 1;
        }
        
        entry = ftn_nodelist_find_by_name(nodelist, argv[3]);
        if (entry) {
            printf("Found entry:\n");
            print_entry(entry);
        } else {
            printf("Name not found: %s\n", argv[3]);
        }
    }
    else if (strcmp(argv[2], "sysop") == 0) {
        if (argc != 4) {
            printf("Usage: %s %s sysop <sysop>\n", argv[0], argv[1]);
            ftn_nodelist_free(nodelist);
            return 1;
        }
        
        entry = ftn_nodelist_find_by_sysop(nodelist, argv[3]);
        if (entry) {
            printf("Found entry:\n");
            print_entry(entry);
        } else {
            printf("Sysop not found: %s\n", argv[3]);
        }
    }
    else if (strcmp(argv[2], "zones") == 0) {
        count = ftn_nodelist_list_zones(nodelist, &list);
        if (count > 0) {
            printf("Zones (%lu):\n", (unsigned long)count);
            for (i = 0; i < count; i++) {
                printf("  %u\n", list[i]);
            }
            free(list);
        } else {
            printf("No zones found\n");
        }
    }
    else if (strcmp(argv[2], "nets") == 0) {
        if (argc != 4) {
            printf("Usage: %s %s nets <zone>\n", argv[0], argv[1]);
            ftn_nodelist_free(nodelist);
            return 1;
        }
        
        count = ftn_nodelist_list_nets(nodelist, atoi(argv[3]), &list);
        if (count > 0) {
            printf("Nets in zone %s (%lu):\n", argv[3], (unsigned long)count);
            for (i = 0; i < count; i++) {
                printf("  %u\n", list[i]);
            }
            free(list);
        } else {
            printf("No nets found in zone %s\n", argv[3]);
        }
    }
    else if (strcmp(argv[2], "nodes") == 0) {
        if (argc != 5) {
            printf("Usage: %s %s nodes <zone> <net>\n", argv[0], argv[1]);
            ftn_nodelist_free(nodelist);
            return 1;
        }
        
        count = ftn_nodelist_list_nodes(nodelist, atoi(argv[3]), atoi(argv[4]), &nodes);
        if (count > 0) {
            printf("Nodes in %s:%s (%lu):\n", argv[3], argv[4], (unsigned long)count);
            for (i = 0; i < count; i++) {
                print_entry(nodes[i]);
            }
            free(nodes);
        } else {
            printf("No nodes found in %s:%s\n", argv[3], argv[4]);
        }
    }
    else {
        printf("Unknown command: %s\n", argv[2]);
        print_usage(argv[0]);
        ftn_nodelist_free(nodelist);
        return 1;
    }
    
    ftn_nodelist_free(nodelist);
    return 0;
}
