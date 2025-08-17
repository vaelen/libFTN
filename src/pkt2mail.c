/*
 * pkt2mail - Convert FidoNet packet files to maildir format
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include <ftn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

static void print_version(void) {
    printf("pkt2mail (libFTN) %s\n", ftn_get_version());
    printf("%s\n", ftn_get_copyright());
    printf("License: %s\n", ftn_get_license());
}

static void print_usage(const char* program_name) {
    printf("Usage: %s [options] <maildir_path> <packet_files...>\n", program_name);
    printf("\n");
    printf("Convert FidoNet packet files to maildir format.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -d, --domain <name>  Domain name for RFC822 addresses (default: fidonet.org)\n");
    printf("  -u, --user <name>    Only process NetMail for specified user (case insensitive)\n");
    printf("  -h, --help           Show this help message\n");
    printf("      --version        Show version information\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  maildir_path     Path to maildir directory (use %%USER%% for per-user folders)\n");
    printf("  packet_files     One or more FidoNet packet (.pkt) files\n");
    printf("\n");
    printf("The maildir directory structure will be created if it doesn't exist.\n");
    printf("Only NetMail messages will be processed.\n");
    printf("If %%USER%% is in the path, it will be replaced with the recipient's name in lowercase.\n");
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

/* Convert string to lowercase */
static void str_tolower(char* str) {
    char* p;
    if (!str) return;
    for (p = str; *p; p++) {
        *p = tolower((unsigned char)*p);
    }
}

/* Replace %USER% in path with actual username */
static char* expand_user_path(const char* path_template, const char* username) {
    const char* user_token = "%USER%";
    const char* pos;
    char* expanded;
    char username_lower[256];
    size_t token_len = strlen(user_token);
    size_t username_len;
    size_t prefix_len;
    size_t suffix_len;
    
    if (!path_template || !username) {
        return NULL;
    }
    
    /* Find %USER% token */
    pos = strstr(path_template, user_token);
    if (!pos) {
        /* No token found, return copy of original path */
        expanded = malloc(strlen(path_template) + 1);
        if (expanded) {
            strlcpy(expanded, path_template, strlen(path_template) + 1);
        }
        return expanded;
    }
    
    /* Convert username to lowercase */
    strlcpy(username_lower, username, sizeof(username_lower));
    str_tolower(username_lower);
    sanitize_filename(username_lower);
    
    username_len = strlen(username_lower);
    prefix_len = pos - path_template;
    suffix_len = strlen(pos + token_len);
    
    /* Allocate expanded path */
    expanded = malloc(prefix_len + username_len + suffix_len + 1);
    if (expanded) {
        /* Copy prefix */
        memcpy(expanded, path_template, prefix_len);
        /* Copy username */
        strlcpy(expanded + prefix_len, username_lower, username_len + 1);
        /* Copy suffix */
        strlcpy(expanded + prefix_len + username_len, pos + token_len, suffix_len + 1);
    }
    
    return expanded;
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
            strlcpy(filename, message->msgid, strlen(message->msgid) + 1);
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
            strlcpy(timestamp_str, "unknown", sizeof(timestamp_str));
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
            strlcpy(timestamp_str, "unknown", sizeof(timestamp_str));
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
static int message_exists(const char* maildir_path, const char* filename, const ftn_message_t* message, const char* path_template) {
    char* actual_maildir_path = NULL;
    int exists = 0;
    char path[512];
    struct stat st;
    DIR* dir;
    struct dirent* entry;
    
    /* Expand user path if needed */
    if (path_template && strstr(path_template, "%USER%")) {
        actual_maildir_path = expand_user_path(path_template, message->to_user);
        if (!actual_maildir_path) {
            return 0;
        }
    } else {
        actual_maildir_path = malloc(strlen(maildir_path) + 1);
        if (actual_maildir_path) {
            strlcpy(actual_maildir_path, maildir_path, strlen(maildir_path) + 1);
        } else {
            return 0;
        }
    }
    
    /* Check in new directory */
    snprintf(path, sizeof(path), "%s/new/%s", actual_maildir_path, filename);
    if (stat(path, &st) == 0) {
        exists = 1;
        goto cleanup;
    }
    
    /* Check in cur directory */
    snprintf(path, sizeof(path), "%s/cur/%s", actual_maildir_path, filename);
    if (stat(path, &st) == 0) {
        exists = 1;
        goto cleanup;
    }
    
    /* Also check for files with additional maildir flags */
    snprintf(path, sizeof(path), "%s/cur", actual_maildir_path);
    dir = opendir(path);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, filename, strlen(filename)) == 0) {
                /* Found a match (possibly with maildir flags) */
                closedir(dir);
                exists = 1;
                goto cleanup;
            }
        }
        closedir(dir);
    }
    
cleanup:
    free(actual_maildir_path);
    return exists;
}

/* Convert message to RFC822 and save to maildir */
static ftn_error_t save_message_to_maildir(const ftn_message_t* message, 
                                          const char* maildir_path,
                                          const char* filename,
                                          const char* domain,
                                          const char* path_template) {
    char* actual_maildir_path = NULL;
    rfc822_message_t* rfc_msg = NULL;
    char* rfc_text = NULL;
    char tmp_path[512];
    char final_path[512];
    FILE* fp;
    ftn_error_t error;
    
    /* Expand user path if needed */
    if (path_template && strstr(path_template, "%USER%")) {
        actual_maildir_path = expand_user_path(path_template, message->to_user);
        if (!actual_maildir_path) {
            fprintf(stderr, "Error: Failed to expand user path\n");
            return FTN_ERROR_NOMEM;
        }
        /* Create maildir structure for user */
        error = create_maildir_structure(actual_maildir_path);
        if (error != FTN_OK) {
            free(actual_maildir_path);
            return error;
        }
    } else {
        actual_maildir_path = malloc(strlen(maildir_path) + 1);
        if (actual_maildir_path) {
            strlcpy(actual_maildir_path, maildir_path, strlen(maildir_path) + 1);
        } else {
            return FTN_ERROR_NOMEM;
        }
    }
    
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
    snprintf(tmp_path, sizeof(tmp_path), "%s/tmp/%s", actual_maildir_path, filename);
    fp = fopen(tmp_path, "w");
    if (!fp) {
        fprintf(stderr, "Error: Failed to create temporary file: %s\n", tmp_path);
        free(rfc_text);
        rfc822_message_free(rfc_msg);
        free(actual_maildir_path);
        return FTN_ERROR_FILE;
    }
    
    if (fputs(rfc_text, fp) == EOF) {
        fprintf(stderr, "Error: Failed to write message to file\n");
        fclose(fp);
        remove(tmp_path);
        free(rfc_text);
        rfc822_message_free(rfc_msg);
        free(actual_maildir_path);
        return FTN_ERROR_FILE;
    }
    
    fclose(fp);
    
    /* Move to new directory */
    snprintf(final_path, sizeof(final_path), "%s/new/%s", actual_maildir_path, filename);
    if (rename(tmp_path, final_path) != 0) {
        fprintf(stderr, "Error: Failed to move message to new directory\n");
        remove(tmp_path);
        free(rfc_text);
        rfc822_message_free(rfc_msg);
        free(actual_maildir_path);
        return FTN_ERROR_FILE;
    }
    
    free(rfc_text);
    rfc822_message_free(rfc_msg);
    free(actual_maildir_path);
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
    const char* user_filter = NULL;
    char** packet_files = NULL;
    int packet_count = 0;
    int arg_index;
    int has_user_template = 0;
    
    /* Parse command line arguments */
    for (arg_index = 1; arg_index < argc; arg_index++) {
        if ((strcmp(argv[arg_index], "-d") == 0 || strcmp(argv[arg_index], "--domain") == 0) && arg_index + 1 < argc) {
            domain = argv[++arg_index];
        } else if ((strcmp(argv[arg_index], "-u") == 0 || strcmp(argv[arg_index], "--user") == 0) && arg_index + 1 < argc) {
            user_filter = argv[++arg_index];
        } else if (strcmp(argv[arg_index], "-h") == 0 || strcmp(argv[arg_index], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[arg_index], "--version") == 0) {
            print_version();
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
        if (strcmp(argv[arg_index], "-d") == 0 || strcmp(argv[arg_index], "--domain") == 0) {
            arg_index++; /* Skip domain value */
        } else if (strcmp(argv[arg_index], "-u") == 0 || strcmp(argv[arg_index], "--user") == 0) {
            arg_index++; /* Skip user value */
        } else if (strcmp(argv[arg_index], "-h") == 0 || strcmp(argv[arg_index], "--help") == 0) {
            /* Skip */
        } else if (strcmp(argv[arg_index], "--version") == 0) {
            /* Skip */
        } else if (argv[arg_index][0] == '-') {
            /* Skip unknown options */
        } else {
            if (strcmp(argv[arg_index], maildir_path) != 0) {
                packet_files[j++] = argv[arg_index];
            }
        }
    }
    
    /* Check if maildir path contains %USER% template */
    has_user_template = (strstr(maildir_path, "%USER%") != NULL);
    
    printf("Converting FidoNet packets to maildir format...\n");
    printf("Maildir path: %s\n", maildir_path);
    printf("Domain: %s\n", domain);
    if (user_filter) {
        printf("User filter: %s\n", user_filter);
    }
    printf("Packet files: %d\n", packet_count);
    for (j = 0; j < (size_t)packet_count; j++) {
        printf("  %s\n", packet_files[j]);
    }
    printf("\n");
    
    /* Create maildir structure only if no %USER% template */
    if (!has_user_template) {
        error = create_maildir_structure(maildir_path);
        if (error != FTN_OK) {
            free(packet_files);
            return 1;
        }
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
            
            /* Apply user filter if specified */
            if (user_filter && strcasecmp(message->to_user, user_filter) != 0) {
                printf("  Skipping message for user %s (filter: %s)\n", message->to_user, user_filter);
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
            if (message_exists(maildir_path, filename, message, has_user_template ? maildir_path : NULL)) {
                printf("  Skipping existing message: %s\n", filename);
                free(filename);
                skipped_count++;
                continue;
            }
            
            /* Save message to maildir */
            error = save_message_to_maildir(message, maildir_path, filename, domain, has_user_template ? maildir_path : NULL);
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