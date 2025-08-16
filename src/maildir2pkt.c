/*
 * maildir2pkt - Convert RFC822 messages to FidoNet packet format
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "../include/ftn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>

static void print_usage(const char* program_name) {
    printf("Usage: %s [options] <rfc822_files...>\n", program_name);
    printf("\n");
    printf("Convert RFC822 message files to FidoNet packet format.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -d <domain>  Domain name for RFC822 addresses (default: fidonet.org)\n");
    printf("  -s <dir>     Move processed files to specified 'Sent' directory\n");
    printf("  -o <file>    Output packet filename (default: auto-generated)\n");
    printf("  -f <addr>    From address (zone:net/node.point format)\n");
    printf("  -t <addr>    To address (zone:net/node.point format)\n");
    printf("  -h           Show this help message\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  rfc822_files One or more RFC822 message files to convert\n");
    printf("\n");
    printf("All messages will be placed into a single packet file.\n");
    printf("If no output filename is specified, an 8-character name will be generated.\n");
}

/* Generate unique 8-character packet filename */
static char* generate_packet_filename(void) {
    char* filename;
    time_t now;
    struct tm* tm_info;
    unsigned int random_part;
    
    filename = malloc(13); /* 8 chars + ".pkt" + null */
    if (!filename) return NULL;
    
    now = time(NULL);
    tm_info = localtime(&now);
    
    /* Generate a pseudo-random number based on current time */
    random_part = (unsigned int)(now & 0xFFFFFF);
    
    if (tm_info) {
        /* Format: MMDDHHNN where NN is random */
        snprintf(filename, 13, "%02d%02d%02d%02x.pkt",
                 tm_info->tm_mon + 1, tm_info->tm_mday,
                 tm_info->tm_hour, (random_part & 0xFF));
    } else {
        /* Fallback to purely random name */
        snprintf(filename, 13, "%08x.pkt", random_part);
    }
    
    return filename;
}

/* Read RFC822 file content */
static char* read_file_content(const char* filename) {
    FILE* fp;
    long file_size;
    char* content;
    size_t bytes_read;
    
    fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }
    
    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error: Cannot seek to end of file %s\n", filename);
        fclose(fp);
        return NULL;
    }
    
    file_size = ftell(fp);
    if (file_size < 0) {
        fprintf(stderr, "Error: Cannot get file size for %s\n", filename);
        fclose(fp);
        return NULL;
    }
    
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Error: Cannot seek to beginning of file %s\n", filename);
        fclose(fp);
        return NULL;
    }
    
    /* Allocate buffer */
    content = malloc(file_size + 1);
    if (!content) {
        fprintf(stderr, "Error: Out of memory reading file %s\n", filename);
        fclose(fp);
        return NULL;
    }
    
    /* Read file */
    bytes_read = fread(content, 1, file_size, fp);
    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "Error: Failed to read complete file %s\n", filename);
        free(content);
        fclose(fp);
        return NULL;
    }
    
    content[file_size] = '\0';
    fclose(fp);
    
    return content;
}

/* Move file to sent directory */
static ftn_error_t move_to_sent(const char* filename, const char* sent_dir) {
    char dest_path[512];
    char* basename;
    struct stat st;
    
    /* Create sent directory if it doesn't exist */
    if (stat(sent_dir, &st) != 0) {
        if (mkdir(sent_dir, 0755) != 0) {
            fprintf(stderr, "Error: Failed to create sent directory: %s\n", sent_dir);
            return FTN_ERROR_FILE;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s exists but is not a directory\n", sent_dir);
        return FTN_ERROR_FILE;
    }
    
    /* Extract basename from filename */
    basename = strrchr(filename, '/');
    if (basename) {
        basename++; /* Skip the '/' */
    } else {
        basename = (char*)filename; /* No path separator found */
    }
    
    /* Construct destination path */
    snprintf(dest_path, sizeof(dest_path), "%s/%s", sent_dir, basename);
    
    /* Move file */
    if (rename(filename, dest_path) != 0) {
        fprintf(stderr, "Error: Failed to move %s to %s\n", filename, dest_path);
        return FTN_ERROR_FILE;
    }
    
    printf("Moved to sent: %s\n", dest_path);
    return FTN_OK;
}

int main(int argc, char* argv[]) {
    ftn_packet_t* packet = NULL;
    ftn_address_t from_addr = {0, 0, 0, 0};
    ftn_address_t to_addr = {0, 0, 0, 0};
    char* output_filename = NULL;
    char* sent_dir = NULL;
    const char* domain = "fidonet.org";
    char** input_files = NULL;
    int input_count = 0;
    int i;
    int processed_count = 0;
    int failed_count = 0;
    int output_filename_allocated = 0;
    ftn_error_t error;
    time_t now;
    struct tm* tm_info;
    
    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -d option requires a domain argument\n");
                return 1;
            }
            domain = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -s option requires a directory argument\n");
                return 1;
            }
            sent_dir = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -o option requires a filename argument\n");
                return 1;
            }
            output_filename = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -f option requires an address argument\n");
                return 1;
            }
            if (!ftn_address_parse(argv[++i], &from_addr)) {
                fprintf(stderr, "Error: Invalid from address format: %s\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-t") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -t option requires an address argument\n");
                return 1;
            }
            if (!ftn_address_parse(argv[++i], &to_addr)) {
                fprintf(stderr, "Error: Invalid to address format: %s\n", argv[i]);
                return 1;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            /* This is an input file */
            if (input_count == 0) {
                input_files = malloc(sizeof(char*) * (argc - i));
                if (!input_files) {
                    fprintf(stderr, "Error: Out of memory\n");
                    return 1;
                }
            }
            input_files[input_count++] = argv[i];
        }
    }
    
    if (input_count == 0) {
        fprintf(stderr, "Error: No input files specified\n");
        print_usage(argv[0]);
        free(input_files);
        return 1;
    }
    
    /* Generate output filename if not specified */
    if (!output_filename) {
        output_filename = generate_packet_filename();
        if (!output_filename) {
            fprintf(stderr, "Error: Failed to generate output filename\n");
            free(input_files);
            return 1;
        }
        output_filename_allocated = 1;
        printf("Generated packet filename: %s\n", output_filename);
    }
    
    printf("Converting %d RFC822 files to FidoNet packet format...\n", input_count);
    printf("Output file: %s\n", output_filename);
    if (sent_dir) {
        printf("Sent directory: %s\n", sent_dir);
    }
    printf("\n");
    
    /* Create new packet */
    packet = ftn_packet_new();
    if (!packet) {
        fprintf(stderr, "Error: Failed to create packet\n");
        free(input_files);
        if (output_filename_allocated) free(output_filename);
        return 1;
    }
    
    /* Set packet header addresses if provided */
    if (from_addr.zone > 0) {
        packet->header.orig_zone = from_addr.zone;
        packet->header.orig_net = from_addr.net;
        packet->header.orig_node = from_addr.node;
    }
    if (to_addr.zone > 0) {
        packet->header.dest_zone = to_addr.zone;
        packet->header.dest_net = to_addr.net;
        packet->header.dest_node = to_addr.node;
    }
    
    /* Set packet creation time */
    now = time(NULL);
    tm_info = localtime(&now);
    if (tm_info) {
        packet->header.year = tm_info->tm_year + 1900;
        packet->header.month = tm_info->tm_mon;
        packet->header.day = tm_info->tm_mday;
        packet->header.hour = tm_info->tm_hour;
        packet->header.minute = tm_info->tm_min;
        packet->header.second = tm_info->tm_sec;
    }
    
    packet->header.packet_type = 0x0002;
    
    /* Process each input file */
    for (i = 0; i < input_count; i++) {
        char* file_content = NULL;
        rfc822_message_t* rfc_msg = NULL;
        ftn_message_t* ftn_msg = NULL;
        
        printf("Processing: %s... ", input_files[i]);
        fflush(stdout);
        
        /* Read file content */
        file_content = read_file_content(input_files[i]);
        if (!file_content) {
            printf("FAILED (read error)\n");
            failed_count++;
            continue;
        }
        
        /* Parse RFC822 message */
        error = rfc822_message_parse(file_content, &rfc_msg);
        if (error != FTN_OK) {
            printf("FAILED (parse error)\n");
            free(file_content);
            failed_count++;
            continue;
        }
        
        /* Convert to FTN message */
        error = rfc822_to_ftn(rfc_msg, domain, &ftn_msg);
        if (error != FTN_OK) {
            printf("FAILED (conversion error)\n");
            rfc822_message_free(rfc_msg);
            free(file_content);
            failed_count++;
            continue;
        }
        
        /* Add to packet */
        error = ftn_packet_add_message(packet, ftn_msg);
        if (error != FTN_OK) {
            printf("FAILED (packet error)\n");
            ftn_message_free(ftn_msg);
            rfc822_message_free(rfc_msg);
            free(file_content);
            failed_count++;
            continue;
        }
        
        printf("OK\n");
        processed_count++;
        
        /* Move to sent directory if requested */
        if (sent_dir) {
            move_to_sent(input_files[i], sent_dir);
        }
        
        rfc822_message_free(rfc_msg);
        free(file_content);
        /* Note: ftn_msg is now owned by the packet and will be freed with it */
    }
    
    /* Save packet */
    if (processed_count > 0) {
        printf("\nSaving packet with %d messages...\n", processed_count);
        error = ftn_packet_save(output_filename, packet);
        if (error != FTN_OK) {
            fprintf(stderr, "Error: Failed to save packet to %s\n", output_filename);
            ftn_packet_free(packet);
            free(input_files);
            if (output_filename_allocated) free(output_filename);
            return 1;
        }
        printf("Packet saved successfully: %s\n", output_filename);
    } else {
        printf("\nNo messages to save.\n");
    }
    
    printf("\nConversion complete:\n");
    printf("  Processed: %d messages\n", processed_count);
    printf("  Failed: %d messages\n", failed_count);
    printf("  Total: %d messages\n", input_count);
    
    ftn_packet_free(packet);
    free(input_files);
    if (output_filename_allocated) {
        free(output_filename);
    }
    
    return (failed_count > 0) ? 1 : 0;
}