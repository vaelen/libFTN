/*
 * pkt2mail - Convert FidoNet packet files to maildir format
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
    printf("Usage: %s [options] <maildir_path> <packet_files...>\n", program_name);
    printf("\n");
    printf("Convert FidoNet packet files to maildir format.\n");
    printf("\n");
    printf("Options:\n");
    printf("  --domain <name>  Domain name for RFC822 addresses (default: fidonet.org)\n");
    printf("  --help           Show this help message\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  maildir_path     Path to maildir directory\n");
    printf("  packet_files     One or more FidoNet packet (.pkt) files\n");
    printf("\n");
    printf("The maildir directory structure will be created if it doesn't exist.\n");
    printf("Only NetMail messages will be processed.\n");
}

/* Create maildir directory structure if it doesn't exist */
static ftn_error_t create_maildir_structure(const char* maildir_path) {
    char path[512];
    struct stat st;
    
    /* Create main maildir directory */
    if (stat(maildir_path, &st) != 0) {
        if (mkdir(maildir_path, 0755) != 0) {
            fprintf(stderr, "Error: Failed to create maildir directory: %s\n", maildir_path);
            return FTN_ERROR_FILE;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s exists but is not a directory\n", maildir_path);
        return FTN_ERROR_FILE;
    }
    
    /* Create tmp subdirectory */
    snprintf(path, sizeof(path), "%s/tmp", maildir_path);
    if (stat(path, &st) != 0) {
        if (mkdir(path, 0755) != 0) {
            fprintf(stderr, "Error: Failed to create tmp directory: %s\n", path);
            return FTN_ERROR_FILE;
        }
    }
    
    /* Create new subdirectory */
    snprintf(path, sizeof(path), "%s/new", maildir_path);
    if (stat(path, &st) != 0) {
        if (mkdir(path, 0755) != 0) {
            fprintf(stderr, "Error: Failed to create new directory: %s\n", path);
            return FTN_ERROR_FILE;
        }
    }
    
    /* Create cur subdirectory */
    snprintf(path, sizeof(path), "%s/cur", maildir_path);
    if (stat(path, &st) != 0) {
        if (mkdir(path, 0755) != 0) {
            fprintf(stderr, "Error: Failed to create cur directory: %s\n", path);
            return FTN_ERROR_FILE;
        }
    }
    
    return FTN_OK;
}

/* Replace special characters in filename with underscores */
static void sanitize_filename(char* filename) {
    char* p;
    
    if (!filename) return;
    
    for (p = filename; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || 
            *p == '?' || *p == '"' || *p == '<' || *p == '>' || 
            *p == '|' || *p == ' ' || *p == '\t' || *p == '\n' ||
            *p == '\r') {
            *p = '_';
        }
    }
}

/* Generate filename for message */
static char* generate_filename(const ftn_message_t* message) {
    char* filename;
    char timestamp_str[32];
    char from_addr[64];
    char to_addr[64];
    struct tm* tm_info;
    
    if (!message) return NULL;
    
    /* Use MSGID if available */
    if (message->msgid && *message->msgid) {
        filename = malloc(strlen(message->msgid) + 1);
        if (filename) {
            strcpy(filename, message->msgid);
            /* Remove angle brackets if present */
            if (filename[0] == '<' && filename[strlen(filename)-1] == '>') {
                memmove(filename, filename + 1, strlen(filename) - 2);
                filename[strlen(filename) - 2] = '\0';
            }
            sanitize_filename(filename);
        }
        return filename;
    }
    
    /* Generate FROM_TO_DATE filename */
    ftn_address_to_string(&message->orig_addr, from_addr, sizeof(from_addr));
    ftn_address_to_string(&message->dest_addr, to_addr, sizeof(to_addr));
    
    if (message->timestamp > 0) {
        tm_info = gmtime(&message->timestamp);
        if (tm_info) {
            snprintf(timestamp_str, sizeof(timestamp_str), "%04d%02d%02d_%02d%02d%02d",
                     tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                     tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
        } else {
            strcpy(timestamp_str, "unknown");
        }
    } else {
        /* Use current time if no timestamp available */
        time_t now = time(NULL);
        tm_info = gmtime(&now);
        if (tm_info) {
            snprintf(timestamp_str, sizeof(timestamp_str), "%04d%02d%02d_%02d%02d%02d",
                     tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                     tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
        } else {
            strcpy(timestamp_str, "unknown");
        }
    }
    
    /* Allocate filename buffer */
    filename = malloc(strlen(from_addr) + strlen(to_addr) + strlen(timestamp_str) + 8);
    if (filename) {
        snprintf(filename, strlen(from_addr) + strlen(to_addr) + strlen(timestamp_str) + 8,
                 "%s_%s_%s", from_addr, to_addr, timestamp_str);
        sanitize_filename(filename);
    }
    
    return filename;
}

/* Check if message already exists in maildir */
static int message_exists(const char* maildir_path, const char* filename) {
    char path[512];
    struct stat st;
    DIR* dir;
    struct dirent* entry;
    
    /* Check in new directory */
    snprintf(path, sizeof(path), "%s/new/%s", maildir_path, filename);
    if (stat(path, &st) == 0) {
        return 1;
    }
    
    /* Check in cur directory */
    snprintf(path, sizeof(path), "%s/cur/%s", maildir_path, filename);
    if (stat(path, &st) == 0) {
        return 1;
    }
    
    /* Also check for files with additional maildir flags */
    snprintf(path, sizeof(path), "%s/cur", maildir_path);
    dir = opendir(path);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, filename, strlen(filename)) == 0) {
                /* Found a match (possibly with maildir flags) */
                closedir(dir);
                return 1;
            }
        }
        closedir(dir);
    }
    
    return 0;
}

/* Convert message to RFC822 and save to maildir */
static ftn_error_t save_message_to_maildir(const ftn_message_t* message, 
                                          const char* maildir_path,
                                          const char* filename,
                                          const char* domain) {
    rfc822_message_t* rfc_msg = NULL;
    char* rfc_text = NULL;
    char tmp_path[512];
    char final_path[512];
    FILE* fp;
    ftn_error_t error;
    
    /* Convert to RFC822 */
    error = ftn_to_rfc822(message, domain, &rfc_msg);
    if (error != FTN_OK) {
        fprintf(stderr, "Error: Failed to convert message to RFC822\n");
        return error;
    }
    
    /* Generate RFC822 text */
    rfc_text = rfc822_message_to_text(rfc_msg);
    if (!rfc_text) {
        fprintf(stderr, "Error: Failed to generate RFC822 text\n");
        rfc822_message_free(rfc_msg);
        return FTN_ERROR_NOMEM;
    }
    
    /* Write to temporary file first */
    snprintf(tmp_path, sizeof(tmp_path), "%s/tmp/%s", maildir_path, filename);
    fp = fopen(tmp_path, "w");
    if (!fp) {
        fprintf(stderr, "Error: Failed to create temporary file: %s\n", tmp_path);
        free(rfc_text);
        rfc822_message_free(rfc_msg);
        return FTN_ERROR_FILE;
    }
    
    if (fputs(rfc_text, fp) == EOF) {
        fprintf(stderr, "Error: Failed to write message to file\n");
        fclose(fp);
        remove(tmp_path);
        free(rfc_text);
        rfc822_message_free(rfc_msg);
        return FTN_ERROR_FILE;
    }
    
    fclose(fp);
    
    /* Move to new directory */
    snprintf(final_path, sizeof(final_path), "%s/new/%s", maildir_path, filename);
    if (rename(tmp_path, final_path) != 0) {
        fprintf(stderr, "Error: Failed to move message to new directory\n");
        remove(tmp_path);
        free(rfc_text);
        rfc822_message_free(rfc_msg);
        return FTN_ERROR_FILE;
    }
    
    free(rfc_text);
    rfc822_message_free(rfc_msg);
    return FTN_OK;
}

int main(int argc, char* argv[]) {
    ftn_packet_t* packet = NULL;
    ftn_error_t error;
    size_t i, j;
    int imported_count = 0;
    int skipped_count = 0;
    int total_packets = 0;
    char* filename;
    const char* domain = "fidonet.org";
    const char* maildir_path = NULL;
    char** packet_files = NULL;
    int packet_count = 0;
    int arg_index;
    
    /* Parse command line arguments */
    for (arg_index = 1; arg_index < argc; arg_index++) {
        if (strcmp(argv[arg_index], "--domain") == 0 && arg_index + 1 < argc) {
            domain = argv[++arg_index];
        } else if (strcmp(argv[arg_index], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[arg_index][0] == '-') {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[arg_index]);
            print_usage(argv[0]);
            return 1;
        } else {
            /* First non-option argument is maildir path */
            if (!maildir_path) {
                maildir_path = argv[arg_index];
            } else {
                /* Remaining arguments are packet files */
                packet_count++;
            }
        }
    }
    
    /* Check for required arguments */
    if (!maildir_path || packet_count == 0) {
        fprintf(stderr, "Error: Missing required arguments\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* Collect packet file names */
    packet_files = malloc(packet_count * sizeof(char*));
    if (!packet_files) {
        fprintf(stderr, "Error: Out of memory\n");
        return 1;
    }
    
    j = 0;
    for (arg_index = 1; arg_index < argc; arg_index++) {
        if (strcmp(argv[arg_index], "--domain") == 0) {
            arg_index++; /* Skip domain value */
        } else if (strcmp(argv[arg_index], "--help") == 0) {
            /* Skip */
        } else if (argv[arg_index][0] == '-') {
            /* Skip unknown options */
        } else {
            if (strcmp(argv[arg_index], maildir_path) != 0) {
                packet_files[j++] = argv[arg_index];
            }
        }
    }
    
    printf("Converting FidoNet packets to maildir format...\n");
    printf("Maildir path: %s\n", maildir_path);
    printf("Domain: %s\n", domain);
    printf("Packet files: %d\n", packet_count);
    for (j = 0; j < (size_t)packet_count; j++) {
        printf("  %s\n", packet_files[j]);
    }
    printf("\n");
    
    /* Create maildir structure */
    error = create_maildir_structure(maildir_path);
    if (error != FTN_OK) {
        free(packet_files);
        return 1;
    }
    
    /* Process each packet file */
    for (j = 0; j < (size_t)packet_count; j++) {
        printf("Processing packet: %s\n", packet_files[j]);
        
        /* Load packet */
        error = ftn_packet_load(packet_files[j], &packet);
        if (error != FTN_OK) {
            fprintf(stderr, "Error: Failed to load packet file: %s\n", packet_files[j]);
            continue;
        }
        
        printf("  Messages in packet: %lu\n", (unsigned long)packet->message_count);
        total_packets++;
        
        /* Process each message */
        for (i = 0; i < packet->message_count; i++) {
            ftn_message_t* message = packet->messages[i];
            
            /* Only process NetMail messages */
            if (message->type != FTN_MSG_NETMAIL) {
                printf("  Skipping echomail message %lu\n", (unsigned long)(i + 1));
                skipped_count++;
                continue;
            }
            
            /* Generate filename */
            filename = generate_filename(message);
            if (!filename) {
                fprintf(stderr, "  Error: Failed to generate filename for message %lu\n", 
                        (unsigned long)(i + 1));
                skipped_count++;
                continue;
            }
            
            /* Check if message already exists */
            if (message_exists(maildir_path, filename)) {
                printf("  Skipping existing message: %s\n", filename);
                free(filename);
                skipped_count++;
                continue;
            }
            
            /* Save message to maildir */
            error = save_message_to_maildir(message, maildir_path, filename, domain);
            if (error == FTN_OK) {
                printf("  Imported message: %s\n", filename);
                imported_count++;
            } else {
                fprintf(stderr, "  Error: Failed to save message: %s\n", filename);
                skipped_count++;
            }
            
            free(filename);
        }
        
        ftn_packet_free(packet);
        packet = NULL;
    }
    
    printf("\nConversion complete:\n");
    printf("  Processed packets: %d\n", total_packets);
    printf("  Imported messages: %d\n", imported_count);
    printf("  Skipped messages: %d\n", skipped_count);
    printf("  Total messages: %d\n", imported_count + skipped_count);
    
    free(packet_files);
    return 0;
}