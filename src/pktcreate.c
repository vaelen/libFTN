/*
 * pktcreate - Create a new FidoNet packet with messages
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "../include/ftn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void print_usage(const char* program_name) {
    printf("Usage: %s [options] <output_file>\n", program_name);
    printf("Create a new FidoNet packet (.pkt) file\n");
    printf("\nOptions:\n");
    printf("  --from-addr <zone:net/node[.point]>  Origin address\n");
    printf("  --to-addr <zone:net/node[.point]>    Destination address\n");
    printf("  --netmail                             Create netmail message\n");
    printf("  --echomail <area>                     Create echomail message for area\n");
    printf("  --from-user <name>                    From user name\n");
    printf("  --to-user <name>                      To user name\n");
    printf("  --subject <text>                      Message subject\n");
    printf("  --text <text>                         Message text\n");
    printf("  --private                             Mark message as private\n");
    printf("  --crash                               Mark message as crash priority\n");
    printf("\nExample (Netmail):\n");
    printf("  %s --from-addr 1:2/3 --to-addr 1:4/5 --netmail \\\n", program_name);
    printf("    --from-user \"John Doe\" --to-user \"Jane Smith\" \\\n");
    printf("    --subject \"Test Message\" --text \"Hello, World!\" test.pkt\n");
    printf("\nExample (Echomail):\n");
    printf("  %s --from-addr 1:2/3 --to-addr 1:4/5 --echomail TEST.AREA \\\n", program_name);
    printf("    --from-user \"John Doe\" --to-user \"All\" \\\n");
    printf("    --subject \"Test Echo\" --text \"Hello, everyone!\" test.pkt\n");
}

static ftn_error_t parse_address(const char* addr_str, ftn_address_t* addr) {
    return ftn_address_parse(addr_str, addr) ? FTN_OK : FTN_ERROR_INVALID_PARAMETER;
}

static void setup_packet_header(ftn_packet_t* packet, 
                                const ftn_address_t* from_addr,
                                const ftn_address_t* to_addr) {
    time_t now;
    struct tm* tm_info;
    
    now = time(NULL);
    tm_info = localtime(&now);
    
    packet->header.orig_zone = from_addr->zone;
    packet->header.orig_net = from_addr->net;
    packet->header.orig_node = from_addr->node;
    packet->header.dest_zone = to_addr->zone;
    packet->header.dest_net = to_addr->net;
    packet->header.dest_node = to_addr->node;
    
    packet->header.year = tm_info->tm_year + 1900;
    packet->header.month = tm_info->tm_mon;
    packet->header.day = tm_info->tm_mday;
    packet->header.hour = tm_info->tm_hour;
    packet->header.minute = tm_info->tm_min;
    packet->header.second = tm_info->tm_sec;
    
    packet->header.packet_type = 0x0002;
    packet->header.baud = 0;
    packet->header.prod_code = 0xFE;  /* Freeware */
    packet->header.serial_no = 0;
    memset(packet->header.password, 0, 8);
    memset(packet->header.fill, 0, 20);
}

int main(int argc, char* argv[]) {
    ftn_packet_t* packet;
    ftn_message_t* message;
    ftn_address_t from_addr, to_addr;
    char* output_file = NULL;
    char* echo_area = NULL;
    char* from_user = NULL;
    char* to_user = NULL;
    char* subject = NULL;
    char* text = NULL;
    int is_netmail = 0;
    int is_echomail = 0;
    int is_private = 0;
    int is_crash = 0;
    int i;
    ftn_error_t result;
    char serial[9];
    
    /* Initialize addresses to invalid values */
    memset(&from_addr, 0, sizeof(from_addr));
    memset(&to_addr, 0, sizeof(to_addr));
    
    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--from-addr") == 0 && i + 1 < argc) {
            if (parse_address(argv[++i], &from_addr) != FTN_OK) {
                printf("Error: Invalid from address: %s\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "--to-addr") == 0 && i + 1 < argc) {
            if (parse_address(argv[++i], &to_addr) != FTN_OK) {
                printf("Error: Invalid to address: %s\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "--netmail") == 0) {
            is_netmail = 1;
        } else if (strcmp(argv[i], "--echomail") == 0 && i + 1 < argc) {
            is_echomail = 1;
            echo_area = argv[++i];
        } else if (strcmp(argv[i], "--from-user") == 0 && i + 1 < argc) {
            from_user = argv[++i];
        } else if (strcmp(argv[i], "--to-user") == 0 && i + 1 < argc) {
            to_user = argv[++i];
        } else if (strcmp(argv[i], "--subject") == 0 && i + 1 < argc) {
            subject = argv[++i];
        } else if (strcmp(argv[i], "--text") == 0 && i + 1 < argc) {
            text = argv[++i];
        } else if (strcmp(argv[i], "--private") == 0) {
            is_private = 1;
        } else if (strcmp(argv[i], "--crash") == 0) {
            is_crash = 1;
        } else if (argv[i][0] != '-') {
            output_file = argv[i];
        } else {
            printf("Error: Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    /* Validate required parameters */
    if (!output_file) {
        printf("Error: Output file not specified\n");
        print_usage(argv[0]);
        return 1;
    }
    
    if (from_addr.zone == 0 || to_addr.zone == 0) {
        printf("Error: Both --from-addr and --to-addr are required\n");
        print_usage(argv[0]);
        return 1;
    }
    
    if (!is_netmail && !is_echomail) {
        printf("Error: Must specify either --netmail or --echomail\n");
        print_usage(argv[0]);
        return 1;
    }
    
    if (is_netmail && is_echomail) {
        printf("Error: Cannot specify both --netmail and --echomail\n");
        return 1;
    }
    
    if (!from_user || !to_user || !subject) {
        printf("Error: --from-user, --to-user, and --subject are required\n");
        return 1;
    }
    
    /* Create packet */
    packet = ftn_packet_new();
    if (!packet) {
        printf("Error: Failed to create packet\n");
        return 1;
    }
    
    setup_packet_header(packet, &from_addr, &to_addr);
    
    /* Create message */
    message = ftn_message_new(is_echomail ? FTN_MSG_ECHOMAIL : FTN_MSG_NETMAIL);
    if (!message) {
        printf("Error: Failed to create message\n");
        ftn_packet_free(packet);
        return 1;
    }
    
    /* Set message fields */
    message->orig_addr = from_addr;
    message->dest_addr = to_addr;
    message->from_user = strdup(from_user);
    message->to_user = strdup(to_user);
    message->subject = strdup(subject);
    message->text = text ? strdup(text) : strdup("");
    
    if (is_echomail) {
        message->area = strdup(echo_area);
        message->tearline = strdup("--- pktcreate 1.0");
        message->origin = malloc(strlen("* Origin: Created with pktcreate (") + 32 + 2);
        if (message->origin) {
            char addr_str[32];
            ftn_address_to_string(&from_addr, addr_str, sizeof(addr_str));
            sprintf(message->origin, "* Origin: Created with pktcreate (%s)", addr_str);
        }
        
        /* Add MSGID for echomail */
        sprintf(serial, "%08lX", (unsigned long)time(NULL));
        ftn_message_set_msgid(message, &from_addr, serial);
    }
    
    /* Set attributes */
    if (is_private) {
        ftn_message_set_attribute(message, FTN_ATTR_PRIVATE);
    }
    if (is_crash) {
        ftn_message_set_attribute(message, FTN_ATTR_CRASH);
    }
    
    /* Add message to packet */
    result = ftn_packet_add_message(packet, message);
    if (result != FTN_OK) {
        printf("Error: Failed to add message to packet\n");
        ftn_message_free(message);
        ftn_packet_free(packet);
        return 1;
    }
    
    /* Save packet */
    printf("Creating packet: %s\n", output_file);
    result = ftn_packet_save(output_file, packet);
    if (result != FTN_OK) {
        printf("Error: Failed to save packet (error %d)\n", result);
        ftn_packet_free(packet);
        return 1;
    }
    
    printf("Packet created successfully with 1 message\n");
    printf("Type: %s\n", is_echomail ? "Echomail" : "Netmail");
    if (is_echomail) {
        printf("Area: %s\n", echo_area);
    }
    printf("From: %s\n", from_user);
    printf("To:   %s\n", to_user);
    printf("Subject: %s\n", subject);
    
    ftn_packet_free(packet);
    return 0;
}
