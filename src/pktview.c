/*
 * pktview - View a specific message from a FidoNet packet file
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "../include/ftn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char* program_name) {
    printf("Usage: %s <packet_file> <message_number>\n", program_name);
    printf("Display a specific message from a FidoNet packet (.pkt) file\n");
    printf("\nExample:\n");
    printf("  %s messages.pkt 1\n", program_name);
}

static const char* message_type_name(ftn_message_type_t type) {
    switch (type) {
        case FTN_MSG_NETMAIL: return "Netmail";
        case FTN_MSG_ECHOMAIL: return "Echomail";
        default: return "Unknown";
    }
}

static void print_message_details(const ftn_message_t* message) {
    char from_addr[32], to_addr[32];
    char timestamp_str[32];
    struct tm* tm_info;
    size_t i;
    
    ftn_address_to_string(&message->orig_addr, from_addr, sizeof(from_addr));
    ftn_address_to_string(&message->dest_addr, to_addr, sizeof(to_addr));
    
    tm_info = localtime(&message->timestamp);
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    printf("Message Details:\n");
    printf("================\n");
    printf("Type:     %s\n", message_type_name(message->type));
    printf("From:     %s (%s)\n", 
           message->from_user ? message->from_user : "(null)", 
           from_addr);
    printf("To:       %s (%s)\n", 
           message->to_user ? message->to_user : "(null)", 
           to_addr);
    printf("Subject:  %s\n", message->subject ? message->subject : "(null)");
    printf("Date:     %s\n", timestamp_str);
    printf("Cost:     %u\n", message->cost);
    
    if (message->type == FTN_MSG_ECHOMAIL && message->area) {
        printf("Area:     %s\n", message->area);
    }
    
    if (message->attributes) {
        printf("Attributes: ");
        if (message->attributes & FTN_ATTR_PRIVATE) printf("Private ");
        if (message->attributes & FTN_ATTR_CRASH) printf("Crash ");
        if (message->attributes & FTN_ATTR_RECD) printf("Received ");
        if (message->attributes & FTN_ATTR_SENT) printf("Sent ");
        if (message->attributes & FTN_ATTR_FILEATTACH) printf("FileAttach ");
        if (message->attributes & FTN_ATTR_INTRANSIT) printf("InTransit ");
        if (message->attributes & FTN_ATTR_ORPHAN) printf("Orphan ");
        if (message->attributes & FTN_ATTR_KILLSENT) printf("KillSent ");
        if (message->attributes & FTN_ATTR_LOCAL) printf("Local ");
        if (message->attributes & FTN_ATTR_HOLDFORPICKUP) printf("HoldForPickup ");
        if (message->attributes & FTN_ATTR_FILEREQUEST) printf("FileRequest ");
        if (message->attributes & FTN_ATTR_RETRECREQ) printf("ReturnReceiptReq ");
        if (message->attributes & FTN_ATTR_ISRETRECEIPT) printf("ReturnReceipt ");
        if (message->attributes & FTN_ATTR_AUDITREQ) printf("AuditReq ");
        if (message->attributes & FTN_ATTR_FILEUPDREQ) printf("FileUpdateReq ");
        printf("\n");
    }
    
    /* Control information */
    if (message->msgid) {
        printf("MSGID:    %s\n", message->msgid);
    }
    
    if (message->reply) {
        printf("REPLY:    %s\n", message->reply);
    }
    
    if (message->tearline) {
        printf("Tearline: %s\n", message->tearline);
    }
    
    if (message->origin) {
        printf("Origin:   %s\n", message->origin);
    }
    
    if (message->seenby_count > 0) {
        printf("SEEN-BY:\n");
        for (i = 0; i < message->seenby_count; i++) {
            printf("  %s\n", message->seenby[i]);
        }
    }
    
    if (message->path_count > 0) {
        printf("PATH:\n");
        for (i = 0; i < message->path_count; i++) {
            printf("  %s\n", message->path[i]);
        }
    }
    
    /* FTS-4001: Addressing Control Paragraphs */
    if (message->fmpt > 0) {
        printf("FMPT:     %u\n", message->fmpt);
    }
    
    if (message->topt > 0) {
        printf("TOPT:     %u\n", message->topt);
    }
    
    if (message->intl) {
        printf("INTL:     %s\n", message->intl);
    }
    
    /* FTS-4008: Time Zone Information */
    if (message->tzutc) {
        printf("TZUTC:    %s\n", message->tzutc);
    }
    
    /* FTS-4000: Generic Control Paragraphs */
    if (message->control_count > 0) {
        printf("Control Lines:\n");
        for (i = 0; i < message->control_count; i++) {
            printf("  %s\n", message->control_lines[i]);
        }
    }
    
    /* FTS-4009: Netmail Tracking */
    if (message->via_count > 0) {
        printf("Via Lines:\n");
        for (i = 0; i < message->via_count; i++) {
            printf("  %s\n", message->via_lines[i]);
        }
    }
    
    printf("\nMessage Text:\n");
    printf("=============\n");
    if (message->text && strlen(message->text) > 0) {
        printf("%s\n", message->text);
    } else {
        printf("(empty)\n");
    }
}

int main(int argc, char* argv[]) {
    ftn_packet_t* packet;
    ftn_error_t result;
    int message_num;
    
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    message_num = atoi(argv[2]);
    if (message_num < 1) {
        printf("Error: Message number must be 1 or greater\n");
        return 1;
    }
    
    printf("Loading packet: %s\n\n", argv[1]);
    
    result = ftn_packet_load(argv[1], &packet);
    if (result != FTN_OK) {
        switch (result) {
            case FTN_ERROR_FILE_NOT_FOUND:
                printf("Error: File not found: %s\n", argv[1]);
                break;
            case FTN_ERROR_INVALID_FORMAT:
                printf("Error: Invalid packet format: %s\n", argv[1]);
                break;
            case FTN_ERROR_MEMORY:
                printf("Error: Out of memory\n");
                break;
            default:
                printf("Error: Failed to load packet (error %d)\n", result);
                break;
        }
        return 1;
    }
    
    if (message_num > (int)packet->message_count) {
        printf("Error: Message number %d not found (packet has %zu messages)\n", 
               message_num, packet->message_count);
        ftn_packet_free(packet);
        return 1;
    }
    
    print_message_details(packet->messages[message_num - 1]);
    
    ftn_packet_free(packet);
    return 0;
}
