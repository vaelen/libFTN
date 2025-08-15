/*
 * pktlist - List messages in a FidoNet packet file
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "../include/ftn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char* program_name) {
    printf("Usage: %s <packet_file1> [packet_file2] ...\n", program_name);
    printf("Lists all messages in FidoNet packet (.pkt) files\n");
    printf("\nExamples:\n");
    printf("  %s messages.pkt\n", program_name);
    printf("  %s *.pkt\n", program_name);
    printf("  %s mail1.pkt mail2.pkt mail3.pkt\n", program_name);
}

static const char* message_type_name(ftn_message_type_t type) {
    switch (type) {
        case FTN_MSG_NETMAIL: return "Netmail";
        case FTN_MSG_ECHOMAIL: return "Echomail";
        default: return "Unknown";
    }
}

static void print_packet_header(const ftn_packet_header_t* header) {
    printf("Packet Header:\n");
    printf("  Type: 0x%04X\n", header->packet_type);
    printf("  From: %u:%u/%u\n", header->orig_zone, header->orig_net, header->orig_node);
    printf("  To:   %u:%u/%u\n", header->dest_zone, header->dest_net, header->dest_node);
    printf("  Date: %04u-%02u-%02u %02u:%02u:%02u\n",
           header->year, header->month + 1, header->day,
           header->hour, header->minute, header->second);
    printf("  Password: %.8s\n", header->password);
    printf("\n");
}

static void print_message_summary(size_t index, const ftn_message_t* message) {
    char from_addr[32], to_addr[32];
    char timestamp_str[32];
    struct tm* tm_info;
    
    ftn_address_to_string(&message->orig_addr, from_addr, sizeof(from_addr));
    ftn_address_to_string(&message->dest_addr, to_addr, sizeof(to_addr));
    
    tm_info = localtime(&message->timestamp);
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    printf("%3zu. %-8s %s -> %s\n", 
           index + 1, 
           message_type_name(message->type),
           from_addr, 
           to_addr);
    
    printf("     From: %s\n", message->from_user ? message->from_user : "(null)");
    printf("     To:   %s\n", message->to_user ? message->to_user : "(null)");
    printf("     Subj: %s\n", message->subject ? message->subject : "(null)");
    printf("     Date: %s\n", timestamp_str);
    
    if (message->type == FTN_MSG_ECHOMAIL && message->area) {
        printf("     Area: %s\n", message->area);
    }
    
    if (message->attributes) {
        printf("     Attr: ");
        if (message->attributes & FTN_ATTR_PRIVATE) printf("Private ");
        if (message->attributes & FTN_ATTR_CRASH) printf("Crash ");
        if (message->attributes & FTN_ATTR_FILEATTACH) printf("FileAttach ");
        if (message->attributes & FTN_ATTR_FILEREQUEST) printf("FileRequest ");
        if (message->attributes & FTN_ATTR_KILLSENT) printf("KillSent ");
        printf("\n");
    }
    
    if (message->msgid) {
        printf("     MSGID: %s\n", message->msgid);
    }
    
    if (message->reply) {
        printf("     REPLY: %s\n", message->reply);
    }
    
    printf("\n");
}

static int process_packet_file(const char* filename, int show_header) {
    ftn_packet_t* packet;
    ftn_error_t result;
    size_t i;
    
    if (show_header) {
        printf("Loading packet: %s\n\n", filename);
    }
    
    result = ftn_packet_load(filename, &packet);
    if (result != FTN_OK) {
        switch (result) {
            case FTN_ERROR_FILE_NOT_FOUND:
                printf("Error: File not found: %s\n", filename);
                break;
            case FTN_ERROR_INVALID_FORMAT:
                printf("Error: Invalid packet format: %s\n", filename);
                break;
            case FTN_ERROR_MEMORY:
                printf("Error: Out of memory\n");
                break;
            default:
                printf("Error: Failed to load packet (error %d)\n", result);
                break;
        }
        return 0;
    }
    
    print_packet_header(&packet->header);
    
    printf("Messages (%zu total):\n\n", packet->message_count);
    
    if (packet->message_count == 0) {
        printf("  (no messages)\n");
    } else {
        for (i = 0; i < packet->message_count; i++) {
            print_message_summary(i, packet->messages[i]);
        }
    }
    
    ftn_packet_free(packet);
    return 1;
}

int main(int argc, char* argv[]) {
    int i;
    int files_processed = 0;
    int files_failed = 0;
    int total_files;
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    total_files = argc - 1;
    
    for (i = 1; i < argc; i++) {
        /* Add separator between files when processing multiple files */
        if (total_files > 1 && i > 1) {
            printf("\n");
            printf("================================================================================\n");
            printf("\n");
        }
        
        if (total_files > 1) {
            printf("File %d of %d: %s\n", i, total_files, argv[i]);
            printf("================================================================================\n\n");
        }
        
        if (process_packet_file(argv[i], total_files == 1)) {
            files_processed++;
        } else {
            files_failed++;
        }
        
        /* Add extra spacing after each file except the last */
        if (i < argc - 1) {
            printf("\n");
        }
    }
    
    /* Print summary for multiple files */
    if (total_files > 1) {
        printf("\n");
        printf("Summary:\n");
        printf("========\n");
        printf("Files processed: %d\n", files_processed);
        if (files_failed > 0) {
            printf("Files failed:    %d\n", files_failed);
        }
        printf("Total files:     %d\n", total_files);
    }
    
    /* Return non-zero if any files failed */
    return (files_failed > 0) ? 1 : 0;
}
