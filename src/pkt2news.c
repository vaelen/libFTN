/*
 * pkt2news - Convert FidoNet Echomail packets to USENET articles
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ftn.h"
#include "ftn/packet.h"
#include "ftn/storage.h"
#include "ftn/version.h"

static void print_version(void) {
    printf("pkt2news (libFTN) %s\n", ftn_get_version());
    printf("%s\n", ftn_get_copyright());
    printf("License: %s\n", ftn_get_license());
}

static void print_usage(const char* program_name) {
    printf("Usage: %s [options] <usenet_root> <packet_files...>\n", program_name);
    printf("\n");
    printf("Convert FidoNet Echomail packets to USENET articles.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -n, --network <network>  Network name for newsgroups (default: fidonet)\n");
    printf("  -h, --help               Show this help message\n");
    printf("      --version            Show version information\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  usenet_root   Root directory for USENET article storage\n");
    printf("  packet_files  One or more FidoNet packet files to convert\n");
    printf("\n");
    printf("Creates directory structure: USENET_ROOT/NETWORK/AREA/ARTICLE_NUM\n");
    printf("Maintains active file with newsgroup information at USENET_ROOT/active\n");
    printf("Only Echomail messages are converted; Netmail messages are skipped.\n");
}




/* Convert and save Echomail message as USENET article */
static int save_usenet_article(const char* usenet_root, const char* network,
                              const ftn_message_t* ftn_msg) {
    ftn_error_t error;

    if (!ftn_msg->area) {
        return 0; /* Skip messages without area */
    }

    /* Use storage library to store the news article */
    error = ftn_storage_store_news_simple(ftn_msg, usenet_root, network);
    return (error == FTN_OK) ? 0 : -1;
}

int main(int argc, char* argv[]) {
    const char* network = "fidonet";
    const char* usenet_root = NULL;
    char** packet_files = NULL;
    int packet_count = 0;
    int i, j;
    int processed_count = 0;
    int echomail_count = 0;
    int netmail_count = 0;
    int failed_count = 0;
    ftn_packet_t* packet;
    ftn_error_t error;
    
    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--network") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s option requires a network argument\n", argv[i]);
                return 1;
            }
            network = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            /* First non-option argument is usenet_root */
            if (!usenet_root) {
                usenet_root = argv[i];
            } else {
                /* Subsequent arguments are packet files */
                if (packet_count == 0) {
                    packet_files = malloc(sizeof(char*) * (argc - i));
                    if (!packet_files) {
                        fprintf(stderr, "Error: Out of memory\n");
                        return 1;
                    }
                }
                packet_files[packet_count++] = argv[i];
            }
        }
    }
    
    if (!usenet_root || packet_count == 0) {
        fprintf(stderr, "Error: Missing required arguments\n");
        print_usage(argv[0]);
        free(packet_files);
        return 1;
    }
    
    printf("Converting %d FidoNet packets to USENET articles...\n", packet_count);
    printf("Network: %s\n", network);
    printf("USENET root: %s\n", usenet_root);
    printf("\n");
    
    /* Process each packet file */
    for (i = 0; i < packet_count; i++) {
        printf("Processing: %s... ", packet_files[i]);
        fflush(stdout);
        
        /* Load packet */
        error = ftn_packet_load(packet_files[i], &packet);
        if (error != FTN_OK) {
            printf("FAILED (load error)\n");
            failed_count++;
            continue;
        }
        
        printf("OK (%d messages)\n", (int)packet->message_count);
        
        /* Process each message in the packet */
        for (j = 0; j < (int)packet->message_count; j++) {
            if (packet->messages[j]->type == FTN_MSG_ECHOMAIL) {
                if (save_usenet_article(usenet_root, network, packet->messages[j]) == 0) {
                    echomail_count++;
                } else {
                    failed_count++;
                }
            } else {
                netmail_count++;
            }
            processed_count++;
        }
        
        ftn_packet_free(packet);
    }
    
    printf("\nConversion complete:\n");
    printf("  Processed packets: %d\n", packet_count);
    printf("  Total messages: %d\n", processed_count);
    printf("  Echomail converted: %d\n", echomail_count);
    printf("  Netmail skipped: %d\n", netmail_count);
    printf("  Failed: %d\n", failed_count);
    
    free(packet_files);
    return (failed_count > 0) ? 1 : 0;
}