/*
 * pkt2news - Convert FidoNet Echomail packets to USENET articles
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "../include/ftn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>

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

/* Get next article number for a newsgroup */
static int get_next_article_number(const char* usenet_root, const char* network, const char* area) {
    char area_path[512];
    DIR* dir;
    struct dirent* entry;
    int max_num = 0;
    int num;
    
    /* Construct area directory path */
    snprintf(area_path, sizeof(area_path), "%s/%s/%s", usenet_root, network, area);
    
    dir = opendir(area_path);
    if (!dir) {
        /* Directory doesn't exist, start with 1 */
        return 1;
    }
    
    /* Find highest existing article number */
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.' && sscanf(entry->d_name, "%d", &num) == 1) {
            if (num > max_num) {
                max_num = num;
            }
        }
    }
    
    closedir(dir);
    return max_num + 1;
}

/* Create directory recursively */
static int create_directory_recursive(const char* path) {
    char* path_copy;
    char* dir_part;
    char* save_ptr;
    char current_path[512];
    struct stat st;
    
    path_copy = malloc(strlen(path) + 1);
    if (!path_copy) return -1;
    strcpy(path_copy, path);
    
    current_path[0] = '\0';
    
    dir_part = strtok_r(path_copy, "/", &save_ptr);
    while (dir_part) {
        if (strlen(current_path) > 0) {
            strcat(current_path, "/");
        }
        strcat(current_path, dir_part);
        
        /* Check if directory exists */
        if (stat(current_path, &st) != 0) {
            /* Create directory */
            if (mkdir(current_path, 0755) != 0) {
                free(path_copy);
                return -1;
            }
        } else if (!S_ISDIR(st.st_mode)) {
            /* Path exists but is not a directory */
            free(path_copy);
            return -1;
        }
        
        dir_part = strtok_r(NULL, "/", &save_ptr);
    }
    
    free(path_copy);
    return 0;
}

/* Update active file */
static int update_active_file(const char* usenet_root, const char* newsgroup, int article_num) {
    char active_path[512];
    char temp_path[512];
    FILE* active_fp;
    FILE* temp_fp;
    char line[1024];
    char existing_newsgroup[256];
    int existing_high, existing_low;
    char existing_perm;
    int found = 0;
    
    snprintf(active_path, sizeof(active_path), "%s/active", usenet_root);
    snprintf(temp_path, sizeof(temp_path), "%s/active.tmp", usenet_root);
    
    /* Open active file for reading (may not exist) */
    active_fp = fopen(active_path, "r");
    temp_fp = fopen(temp_path, "w");
    if (!temp_fp) {
        if (active_fp) fclose(active_fp);
        return -1;
    }
    
    /* Copy existing entries, updating the newsgroup if found */
    if (active_fp) {
        while (fgets(line, sizeof(line), active_fp)) {
            if (sscanf(line, "%255s %d %d %c", existing_newsgroup, &existing_high, &existing_low, &existing_perm) == 4) {
                if (strcmp(existing_newsgroup, newsgroup) == 0) {
                    /* Update this newsgroup */
                    if (article_num > existing_high) {
                        existing_high = article_num;
                    }
                    if (existing_low == 0 || article_num < existing_low) {
                        existing_low = article_num;
                    }
                    fprintf(temp_fp, "%s %d %d %c\n", newsgroup, existing_high, existing_low, existing_perm);
                    found = 1;
                } else {
                    /* Copy existing entry */
                    fputs(line, temp_fp);
                }
            } else {
                /* Copy malformed line as-is */
                fputs(line, temp_fp);
            }
        }
        fclose(active_fp);
    }
    
    /* Add new newsgroup if not found */
    if (!found) {
        fprintf(temp_fp, "%s %d %d y\n", newsgroup, article_num, article_num);
    }
    
    fclose(temp_fp);
    
    /* Replace active file with temp file */
    if (rename(temp_path, active_path) != 0) {
        unlink(temp_path);
        return -1;
    }
    
    return 0;
}

/* Convert and save Echomail message as USENET article */
static int save_usenet_article(const char* usenet_root, const char* network, 
                              const ftn_message_t* ftn_msg) {
    rfc822_message_t* usenet_msg = NULL;
    char* newsgroup;
    char* article_text;
    char area_path[512];
    char article_path[512];
    char* lowercase_area;
    FILE* fp;
    int article_num;
    int result = 0;
    size_t i;
    
    if (!ftn_msg->area) {
        return 0; /* Skip messages without area */
    }
    
    /* Convert to lowercase area name */
    lowercase_area = malloc(strlen(ftn_msg->area) + 1);
    if (!lowercase_area) return -1;
    
    for (i = 0; i < strlen(ftn_msg->area); i++) {
        lowercase_area[i] = tolower(ftn_msg->area[i]);
    }
    lowercase_area[strlen(ftn_msg->area)] = '\0';
    
    /* Create newsgroup name */
    newsgroup = ftn_area_to_newsgroup(network, ftn_msg->area);
    if (!newsgroup) {
        free(lowercase_area);
        return -1;
    }
    
    /* Convert FTN message to USENET article */
    if (ftn_to_usenet(ftn_msg, network, &usenet_msg) != FTN_OK) {
        free(newsgroup);
        free(lowercase_area);
        return -1;
    }
    
    /* Create area directory */
    snprintf(area_path, sizeof(area_path), "%s/%s/%s", usenet_root, network, lowercase_area);
    if (create_directory_recursive(area_path) != 0) {
        rfc822_message_free(usenet_msg);
        free(newsgroup);
        free(lowercase_area);
        return -1;
    }
    
    /* Get next article number */
    article_num = get_next_article_number(usenet_root, network, lowercase_area);
    
    /* Create article file */
    snprintf(article_path, sizeof(article_path), "%s/%d", area_path, article_num);
    fp = fopen(article_path, "w");
    if (!fp) {
        rfc822_message_free(usenet_msg);
        free(newsgroup);
        free(lowercase_area);
        return -1;
    }
    
    /* Generate article text */
    article_text = rfc822_message_to_text(usenet_msg);
    if (article_text) {
        fprintf(fp, "%s", article_text);
        free(article_text);
    }
    
    fclose(fp);
    
    /* Update active file */
    if (update_active_file(usenet_root, newsgroup, article_num) != 0) {
        result = -1;
    }
    
    rfc822_message_free(usenet_msg);
    free(newsgroup);
    free(lowercase_area);
    
    return result;
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