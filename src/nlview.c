/*
 * nlview - Nodelist Viewer
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 * 
 * Example program to display the contents of a FidoNet nodelist
 */

#include "ftn.h"

static void print_version(void) {
    printf("nlview (libFTN) %s\n", ftn_get_version());
    printf("%s\n", ftn_get_copyright());
    printf("License: %s\n", ftn_get_license());
}

static void print_usage(const char* progname) {
    printf("Usage: %s [options] <nodelist_file>\n", progname);
    printf("Display the contents of a FidoNet nodelist file\n");
    printf("\nOptions:\n");
    printf("  -h, --help     Show this help message\n");
    printf("      --version  Show version information\n");
}

static void print_entry(const ftn_nodelist_entry_t* entry) {
    char addr_str[32];
    
    ftn_address_to_string(&entry->address, addr_str, sizeof(addr_str));
    
    printf("%-8s %-12s %-20s %-15s %-20s %-15s %-6s %s\n",
           ftn_node_type_to_string(entry->type),
           addr_str,
           entry->name ? entry->name : "",
           entry->location ? entry->location : "",
           entry->sysop ? entry->sysop : "",
           entry->phone ? entry->phone : "",
           entry->speed ? entry->speed : "",
           entry->flags ? entry->flags : "");
}

int main(int argc, char* argv[]) {
    ftn_nodelist_t* nodelist;
    ftn_error_t result;
    size_t i;
    char* nodelist_file = NULL;
    int arg_index;
    
    /* Parse command line arguments */
    for (arg_index = 1; arg_index < argc; arg_index++) {
        if (strcmp(argv[arg_index], "-h") == 0 || strcmp(argv[arg_index], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[arg_index], "--version") == 0) {
            print_version();
            return 0;
        } else if (argv[arg_index][0] == '-') {
            printf("Error: Unknown option: %s\n", argv[arg_index]);
            print_usage(argv[0]);
            return 1;
        } else {
            if (!nodelist_file) {
                nodelist_file = argv[arg_index];
            } else {
                printf("Error: Too many arguments\n");
                print_usage(argv[0]);
                return 1;
            }
        }
    }
    
    if (!nodelist_file) {
        printf("Error: Missing nodelist file\n");
        print_usage(argv[0]);
        return 1;
    }
    
    printf("Loading nodelist: %s\n", nodelist_file);
    
    result = ftn_nodelist_load(nodelist_file, &nodelist);
    if (result != FTN_OK) {
        switch (result) {
            case FTN_ERROR_FILE:
                printf("Error: Cannot open file '%s'\n", nodelist_file);
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
    
    printf("\nTitle: %s\n", nodelist->title ? nodelist->title : "Unknown");
    printf("CRC: %u\n", nodelist->crc);
    printf("Entries: %lu\n\n", (unsigned long)nodelist->count);
    
    printf("%-8s %-12s %-20s %-15s %-20s %-15s %-6s %s\n",
           "Type", "Address", "Name", "Location", "Sysop", "Phone", "Speed", "Flags");
    printf("%-8s %-12s %-20s %-15s %-20s %-15s %-6s %s\n",
           "--------", "------------", "--------------------", "---------------", 
           "--------------------", "---------------", "------", "-----");
    
    for (i = 0; i < nodelist->count; i++) {
        print_entry(nodelist->entries[i]);
    }
    
    printf("\nTotal entries: %lu\n", (unsigned long)nodelist->count);
    
    ftn_nodelist_free(nodelist);
    return 0;
}
