/*
 * pktbundle - Bundle multiple packet files into a single packet
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include <ftn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void print_version(void) {
    printf("pktbundle (libFTN) %s\n", ftn_get_version());
    printf("%s\n", ftn_get_copyright());
    printf("License: %s\n", ftn_get_license());
}

static void print_usage(const char* program_name) {
    printf("Usage: %s [options] <output_file> <input_file1> [input_file2] ...\n", program_name);
    printf("Bundle multiple FidoNet packet files into a single packet\n");
    printf("\nOptions:\n");
    printf("  --from-addr <zone:net/node[.point]>  Origin address for bundle\n");
    printf("  --to-addr <zone:net/node[.point]>    Destination address for bundle\n");
    printf("  -h, --help                           Show this help message\n");
    printf("      --version                        Show version information\n");
    printf("\nExample:\n");
    printf("  %s --from-addr 1:2/3 --to-addr 1:4/5 bundle.pkt msg1.pkt msg2.pkt\n", program_name);
    printf("\nNote: If addresses are not specified, they will be taken from the first packet\n");
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

static ftn_message_t* copy_message(const ftn_message_t* src) {
    ftn_message_t* dst;
    size_t i;
    
    dst = ftn_message_new(src->type);
    if (!dst) return NULL;
    
    /* Copy basic fields */
    dst->orig_addr = src->orig_addr;
    dst->dest_addr = src->dest_addr;
    dst->attributes = src->attributes;
    dst->cost = src->cost;
    dst->timestamp = src->timestamp;
    
    /* Copy strings */
    if (src->to_user) dst->to_user = strdup(src->to_user);
    if (src->from_user) dst->from_user = strdup(src->from_user);
    if (src->subject) dst->subject = strdup(src->subject);
    if (src->text) dst->text = strdup(src->text);
    if (src->area) dst->area = strdup(src->area);
    if (src->origin) dst->origin = strdup(src->origin);
    if (src->tearline) dst->tearline = strdup(src->tearline);
    if (src->msgid) dst->msgid = strdup(src->msgid);
    if (src->reply) dst->reply = strdup(src->reply);
    
    /* Copy SEEN-BY lines */
    for (i = 0; i < src->seenby_count; i++) {
        if (src->seenby[i]) {
            ftn_message_add_seenby(dst, src->seenby[i]);
        }
    }
    
    /* Copy PATH lines */
    for (i = 0; i < src->path_count; i++) {
        if (src->path[i]) {
            ftn_message_add_path(dst, src->path[i]);
        }
    }
    
    return dst;
}

int main(int argc, char* argv[]) {
    ftn_packet_t* output_packet;
    ftn_packet_t* input_packet;
    ftn_address_t from_addr, to_addr;
    char* output_file = NULL;
    char** input_files = NULL;
    int input_count = 0;
    int have_addresses = 0;
    int i, j;
    ftn_error_t result;
    size_t total_messages = 0;
    
    /* Initialize addresses */
    memset(&from_addr, 0, sizeof(from_addr));
    memset(&to_addr, 0, sizeof(to_addr));
    
    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            if (input_files) free(input_files);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            print_version();
            if (input_files) free(input_files);
            return 0;
        } else if (strcmp(argv[i], "--from-addr") == 0 && i + 1 < argc) {
            if (parse_address(argv[++i], &from_addr) != FTN_OK) {
                printf("Error: Invalid from address: %s\n", argv[i]);
                return 1;
            }
            have_addresses |= 1;
        } else if (strcmp(argv[i], "--to-addr") == 0 && i + 1 < argc) {
            if (parse_address(argv[++i], &to_addr) != FTN_OK) {
                printf("Error: Invalid to address: %s\n", argv[i]);
                return 1;
            }
            have_addresses |= 2;
        } else if (argv[i][0] != '-') {
            if (!output_file) {
                output_file = argv[i];
            } else {
                if (!input_files) {
                    input_files = malloc((argc - i) * sizeof(char*));
                    if (!input_files) {
                        printf("Error: Out of memory\n");
                        return 1;
                    }
                }
                input_files[input_count++] = argv[i];
            }
        } else {
            printf("Error: Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            if (input_files) free(input_files);
            return 1;
        }
    }
    
    /* Validate parameters */
    if (!output_file || input_count == 0) {
        printf("Error: Must specify output file and at least one input file\n");
        print_usage(argv[0]);
        if (input_files) free(input_files);
        return 1;
    }
    
    /* Create output packet */
    output_packet = ftn_packet_new();
    if (!output_packet) {
        printf("Error: Failed to create output packet\n");
        if (input_files) free(input_files);
        return 1;
    }
    
    printf("Bundling %d packet files into: %s\n", input_count, output_file);
    
    /* Process input packets */
    for (i = 0; i < input_count; i++) {
        printf("Processing: %s\n", input_files[i]);
        
        result = ftn_packet_load(input_files[i], &input_packet);
        if (result != FTN_OK) {
            printf("Warning: Failed to load %s (error %d), skipping\n", 
                   input_files[i], result);
            continue;
        }
        
        /* Use addresses from first packet if not specified */
        if (i == 0) {
            if (!(have_addresses & 1)) {
                from_addr.zone = input_packet->header.orig_zone;
                from_addr.net = input_packet->header.orig_net;
                from_addr.node = input_packet->header.orig_node;
                from_addr.point = 0;
            }
            if (!(have_addresses & 2)) {
                to_addr.zone = input_packet->header.dest_zone;
                to_addr.net = input_packet->header.dest_net;
                to_addr.node = input_packet->header.dest_node;
                to_addr.point = 0;
            }
            setup_packet_header(output_packet, &from_addr, &to_addr);
        }
        
        /* Copy all messages from input packet */
        for (j = 0; j < (int)input_packet->message_count; j++) {
            ftn_message_t* copied_message = copy_message(input_packet->messages[j]);
            if (copied_message) {
                result = ftn_packet_add_message(output_packet, copied_message);
                if (result == FTN_OK) {
                    total_messages++;
                } else {
                    printf("Warning: Failed to add message %d from %s\n", 
                           j + 1, input_files[i]);
                    ftn_message_free(copied_message);
                }
            } else {
                printf("Warning: Failed to copy message %d from %s\n", 
                       j + 1, input_files[i]);
            }
        }
        
        printf("  Added %zu messages\n", input_packet->message_count);
        ftn_packet_free(input_packet);
    }
    
    if (total_messages == 0) {
        printf("Error: No messages to bundle\n");
        ftn_packet_free(output_packet);
        if (input_files) free(input_files);
        return 1;
    }
    
    /* Save output packet */
    result = ftn_packet_save(output_file, output_packet);
    if (result != FTN_OK) {
        printf("Error: Failed to save output packet (error %d)\n", result);
        ftn_packet_free(output_packet);
        if (input_files) free(input_files);
        return 1;
    }
    
    printf("\nBundle created successfully:\n");
    printf("  Output file: %s\n", output_file);
    printf("  Total messages: %zu\n", total_messages);
    printf("  From: %u:%u/%u\n", from_addr.zone, from_addr.net, from_addr.node);
    printf("  To:   %u:%u/%u\n", to_addr.zone, to_addr.net, to_addr.node);
    
    ftn_packet_free(output_packet);
    if (input_files) free(input_files);
    return 0;
}
