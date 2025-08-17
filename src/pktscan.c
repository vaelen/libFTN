/*
 * pktscan - Process incoming FidoNet packets
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include <ftn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>

/* Configuration structure */
typedef struct {
    /* System paths */
    char* pkt2mail_path;
    char* pkt2news_path;
    char* msg2pkt_path;
    
    /* Node information */
    char* node_name;
    char* sysop;
    char* sysop_name;
    char* email;
    char* www;
    char* telnet;
    char** networks;
    int network_count;
    
    /* News and mail paths */
    char* news_path;
    char* mail_path;
    char* mail_sent;
    
    /* Network configurations */
    struct {
        char* name;
        char* domain;
        char* address;
        char* hub;
        char* inbox;
        char* outbox;
        char* processed;
    } *network_configs;
    int network_config_count;
    
    /* Continuous mode */
    int continuous_mode;
    int sleep_seconds;
} config_t;

/* Global config and running flag */
static config_t g_config;
static volatile int g_running = 1;

/* Signal handler for graceful shutdown */
static void signal_handler(int sig) {
    (void)sig; /* Unused parameter */
    g_running = 0;
    printf("\nReceived signal, shutting down gracefully...\n");
}

static void print_version(void) {
    printf("pktscan (libFTN) %s\n", ftn_get_version());
    printf("%s\n", ftn_get_copyright());
    printf("License: %s\n", ftn_get_license());
}

static void print_usage(const char* program_name) {
    printf("Usage: %s [options] <config_file>\n", program_name);
    printf("\n");
    printf("Process incoming FidoNet packets based on configuration.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c, --continuous     Run in continuous mode (don't exit)\n");
    printf("  -s, --sleep <secs>   Sleep interval in continuous mode (default: 60)\n");
    printf("  -h, --help           Show this help message\n");
    printf("      --version        Show version information\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  config_file    Path to INI configuration file\n");
    printf("\n");
    printf("The configuration file specifies network settings and processing paths.\n");
}

/* Convert string to lowercase */
static void str_tolower(char* str) {
    char* p;
    if (!str) return;
    for (p = str; *p; p++) {
        *p = tolower((unsigned char)*p);
    }
}

/* Trim whitespace from string */
static char* str_trim(char* str) {
    char* start;
    char* end;
    
    if (!str || !*str) return str;
    
    /* Trim leading whitespace */
    start = str;
    while (isspace((unsigned char)*start)) start++;
    
    /* All spaces? */
    if (!*start) {
        *str = '\0';
        return str;
    }
    
    /* Trim trailing whitespace */
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    
    /* Move trimmed string to beginning if needed */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    
    return str;
}

/* Parse a line from INI file */
static int parse_ini_line(const char* line, char** section, char** key, char** value) {
    char* work_line;
    char* p;
    char* equals;
    
    *section = NULL;
    *key = NULL;
    *value = NULL;
    
    if (!line || !*line) return 0;
    
    /* Make a working copy */
    work_line = strdup(line);
    if (!work_line) return 0;
    
    /* Trim the line */
    str_trim(work_line);
    
    /* Skip empty lines and comments */
    if (!*work_line || *work_line == ';' || *work_line == '#') {
        free(work_line);
        return 0;
    }
    
    /* Check for section header */
    if (*work_line == '[') {
        p = strchr(work_line, ']');
        if (p) {
            *p = '\0';
            *section = strdup(work_line + 1);
            str_tolower(*section);
        }
        free(work_line);
        return *section != NULL;
    }
    
    /* Parse key=value */
    equals = strchr(work_line, '=');
    if (equals) {
        *equals = '\0';
        *key = strdup(work_line);
        *value = strdup(equals + 1);
        
        if (*key) {
            str_trim(*key);
            str_tolower(*key);
        }
        if (*value) {
            str_trim(*value);
        }
    }
    
    free(work_line);
    return (*key != NULL && *value != NULL);
}

/* Free configuration structure */
static void free_config(config_t* config) {
    int i;
    
    if (!config) return;
    
    free(config->pkt2mail_path);
    free(config->pkt2news_path);
    free(config->msg2pkt_path);
    free(config->node_name);
    free(config->sysop);
    free(config->sysop_name);
    free(config->email);
    free(config->www);
    free(config->telnet);
    free(config->news_path);
    free(config->mail_path);
    free(config->mail_sent);
    
    if (config->networks) {
        for (i = 0; i < config->network_count; i++) {
            free(config->networks[i]);
        }
        free(config->networks);
    }
    
    if (config->network_configs) {
        for (i = 0; i < config->network_config_count; i++) {
            free(config->network_configs[i].name);
            free(config->network_configs[i].domain);
            free(config->network_configs[i].address);
            free(config->network_configs[i].hub);
            free(config->network_configs[i].inbox);
            free(config->network_configs[i].outbox);
            free(config->network_configs[i].processed);
        }
        free(config->network_configs);
    }
    
    memset(config, 0, sizeof(config_t));
}

/* Load configuration from INI file */
static int load_config(const char* filename, config_t* config) {
    FILE* fp;
    char line[512];
    char* current_section = NULL;
    char* section;
    char* key;
    char* value;
    int network_index = -1;
    
    if (!filename || !config) return 0;
    
    fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open configuration file: %s\n", filename);
        return 0;
    }
    
    /* Initialize config with defaults */
    memset(config, 0, sizeof(config_t));
    config->sleep_seconds = 60; /* Default sleep interval */
    
    while (fgets(line, sizeof(line), fp)) {
        /* Remove newline */
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        nl = strchr(line, '\r');
        if (nl) *nl = '\0';
        
        if (parse_ini_line(line, &section, &key, &value)) {
            if (section) {
                /* New section */
                free(current_section);
                current_section = section;
                
                /* Check if this is a network section */
                if (strcmp(current_section, "system") != 0 &&
                    strcmp(current_section, "node") != 0 &&
                    strcmp(current_section, "news") != 0 &&
                    strcmp(current_section, "mail") != 0) {
                    /* This is a network section */
                    network_index = config->network_config_count;
                    config->network_config_count++;
                    config->network_configs = realloc(config->network_configs,
                        config->network_config_count * sizeof(config->network_configs[0]));
                    if (config->network_configs) {
                        memset(&config->network_configs[network_index], 0,
                               sizeof(config->network_configs[0]));
                    }
                } else {
                    network_index = -1;
                }
            } else if (key && value && current_section) {
                /* Key-value pair in current section */
                if (strcmp(current_section, "system") == 0) {
                    if (strcmp(key, "pkt2mail") == 0) {
                        config->pkt2mail_path = strdup(value);
                    } else if (strcmp(key, "pkt2news") == 0) {
                        config->pkt2news_path = strdup(value);
                    } else if (strcmp(key, "msg2pkt") == 0) {
                        config->msg2pkt_path = strdup(value);
                    }
                } else if (strcmp(current_section, "node") == 0) {
                    if (strcmp(key, "name") == 0) {
                        config->node_name = strdup(value);
                    } else if (strcmp(key, "networks") == 0) {
                        /* Parse comma-separated network list */
                        char* networks_copy = strdup(value);
                        char* token;
                        char* saveptr = NULL;
                        
                        token = strtok_r(networks_copy, ",", &saveptr);
                        while (token) {
                            str_trim(token);
                            config->network_count++;
                            config->networks = realloc(config->networks,
                                config->network_count * sizeof(char*));
                            if (config->networks) {
                                config->networks[config->network_count - 1] = strdup(token);
                            }
                            token = strtok_r(NULL, ",", &saveptr);
                        }
                        free(networks_copy);
                    } else if (strcmp(key, "sysop") == 0) {
                        config->sysop = strdup(value);
                    } else if (strcmp(key, "sysop_name") == 0) {
                        config->sysop_name = strdup(value);
                    } else if (strcmp(key, "email") == 0) {
                        config->email = strdup(value);
                    } else if (strcmp(key, "www") == 0) {
                        config->www = strdup(value);
                    } else if (strcmp(key, "telnet") == 0) {
                        config->telnet = strdup(value);
                    }
                } else if (strcmp(current_section, "news") == 0) {
                    if (strcmp(key, "path") == 0) {
                        config->news_path = strdup(value);
                    }
                } else if (strcmp(current_section, "mail") == 0) {
                    if (strcmp(key, "path") == 0) {
                        config->mail_path = strdup(value);
                    } else if (strcmp(key, "sent") == 0) {
                        config->mail_sent = strdup(value);
                    }
                } else if (network_index >= 0 && network_index < config->network_config_count) {
                    /* Network section */
                    if (strcmp(key, "name") == 0) {
                        config->network_configs[network_index].name = strdup(value);
                    } else if (strcmp(key, "domain") == 0) {
                        config->network_configs[network_index].domain = strdup(value);
                    } else if (strcmp(key, "address") == 0) {
                        config->network_configs[network_index].address = strdup(value);
                    } else if (strcmp(key, "hub") == 0) {
                        config->network_configs[network_index].hub = strdup(value);
                    } else if (strcmp(key, "inbox") == 0) {
                        config->network_configs[network_index].inbox = strdup(value);
                    } else if (strcmp(key, "outbox") == 0) {
                        config->network_configs[network_index].outbox = strdup(value);
                    } else if (strcmp(key, "processed") == 0) {
                        config->network_configs[network_index].processed = strdup(value);
                    }
                }
                
                free(key);
                free(value);
            }
        }
    }
    
    free(current_section);
    fclose(fp);
    
    /* Validate configuration */
    if (!config->pkt2mail_path || !config->pkt2news_path) {
        fprintf(stderr, "Error: Missing required system paths in configuration\n");
        return 0;
    }
    
    if (config->network_count == 0) {
        fprintf(stderr, "Error: No networks configured\n");
        return 0;
    }
    
    return 1;
}

/* Move file to processed directory */
static int move_to_processed(const char* filename, const char* processed_dir) {
    char dest_path[512];
    char* basename;
    struct stat st;
    
    /* Create processed directory if it doesn't exist */
    if (stat(processed_dir, &st) != 0) {
        if (mkdir(processed_dir, 0755) != 0) {
            fprintf(stderr, "Error: Failed to create processed directory: %s\n", processed_dir);
            return 0;
        }
    }
    
    /* Extract basename */
    basename = strrchr(filename, '/');
    if (basename) {
        basename++;
    } else {
        basename = (char*)filename;
    }
    
    /* Construct destination path */
    snprintf(dest_path, sizeof(dest_path), "%s/%s", processed_dir, basename);
    
    /* Move file */
    if (rename(filename, dest_path) != 0) {
        fprintf(stderr, "Error: Failed to move %s to %s\n", filename, dest_path);
        return 0;
    }
    
    return 1;
}

/* Process packets in a network's inbox */
static int process_network_inbox(config_t* config, int network_index) {
    DIR* dir;
    struct dirent* entry;
    char filepath[512];
    char command[2048];
    int processed_count = 0;
    int result;
    
    if (!config || network_index >= config->network_config_count) {
        return 0;
    }
    
    printf("Processing inbox for %s...\n", config->network_configs[network_index].name);
    
    /* Open inbox directory */
    dir = opendir(config->network_configs[network_index].inbox);
    if (!dir) {
        fprintf(stderr, "Warning: Cannot open inbox directory: %s\n",
                config->network_configs[network_index].inbox);
        return 0;
    }
    
    /* Process each .pkt file */
    while ((entry = readdir(dir)) != NULL) {
        /* Check if it's a .pkt file */
        if (strlen(entry->d_name) >= 4 &&
            strcmp(entry->d_name + strlen(entry->d_name) - 4, ".pkt") == 0) {
            
            /* Construct full path */
            snprintf(filepath, sizeof(filepath), "%s/%s",
                     config->network_configs[network_index].inbox, entry->d_name);
            
            printf("  Processing packet: %s\n", entry->d_name);
            
            /* Run pkt2mail if mail path is configured */
            if (config->mail_path) {
                snprintf(command, sizeof(command), "%s --domain %s \"%s\" \"%s\"",
                         config->pkt2mail_path,
                         config->network_configs[network_index].domain,
                         config->mail_path,
                         filepath);
                
                printf("    Running: %s\n", command);
                result = system(command);
                if (result != 0) {
                    fprintf(stderr, "    Warning: pkt2mail returned non-zero status: %d\n", result);
                }
            }
            
            /* Run pkt2news if news path is configured */
            if (config->news_path) {
                snprintf(command, sizeof(command), "%s -n %s \"%s\" \"%s\"",
                         config->pkt2news_path,
                         config->network_configs[network_index].name,
                         config->news_path,
                         filepath);
                
                printf("    Running: %s\n", command);
                result = system(command);
                if (result != 0) {
                    fprintf(stderr, "    Warning: pkt2news returned non-zero status: %d\n", result);
                }
            }
            
            /* Move to processed directory */
            if (config->network_configs[network_index].processed) {
                if (move_to_processed(filepath, config->network_configs[network_index].processed)) {
                    printf("    Moved to processed directory\n");
                } else {
                    fprintf(stderr, "    Warning: Failed to move to processed directory\n");
                }
            }
            
            processed_count++;
        }
    }
    
    closedir(dir);
    
    if (processed_count > 0) {
        printf("  Processed %d packets for %s\n", processed_count,
               config->network_configs[network_index].name);
    } else {
        printf("  No packets found for %s\n", config->network_configs[network_index].name);
    }
    
    return processed_count;
}

/* Process all network inboxes */
static int process_all_inboxes(config_t* config) {
    int i;
    int total_processed = 0;
    
    if (!config) return 0;
    
    printf("Scanning for incoming packets...\n");
    
    for (i = 0; i < config->network_config_count; i++) {
        if (config->network_configs[i].inbox) {
            total_processed += process_network_inbox(config, i);
        }
    }
    
    printf("Total packets processed: %d\n", total_processed);
    return total_processed;
}

int main(int argc, char* argv[]) {
    char* config_file = NULL;
    int i;
    int continuous_mode = 0;
    int sleep_seconds = 60;
    
    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--continuous") == 0) {
            continuous_mode = 1;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--sleep") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s option requires a seconds argument\n", argv[i]);
                return 1;
            }
            sleep_seconds = atoi(argv[++i]);
            if (sleep_seconds <= 0) {
                fprintf(stderr, "Error: Invalid sleep interval: %s\n", argv[i]);
                return 1;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            /* Configuration file */
            if (!config_file) {
                config_file = argv[i];
            } else {
                fprintf(stderr, "Error: Multiple configuration files specified\n");
                print_usage(argv[0]);
                return 1;
            }
        }
    }
    
    if (!config_file) {
        fprintf(stderr, "Error: Configuration file is required\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* Load configuration */
    if (!load_config(config_file, &g_config)) {
        fprintf(stderr, "Error: Failed to load configuration from %s\n", config_file);
        return 1;
    }
    
    g_config.continuous_mode = continuous_mode;
    g_config.sleep_seconds = sleep_seconds;
    
    printf("pktscan started\n");
    printf("Configuration file: %s\n", config_file);
    printf("Mode: %s\n", continuous_mode ? "continuous" : "single shot");
    if (continuous_mode) {
        printf("Sleep interval: %d seconds\n", sleep_seconds);
    }
    printf("\n");
    
    /* Set up signal handlers for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Main processing loop */
    do {
        process_all_inboxes(&g_config);
        
        if (continuous_mode && g_running) {
            printf("\nSleeping for %d seconds...\n", sleep_seconds);
            for (i = 0; i < sleep_seconds && g_running; i++) {
                sleep(1);
            }
        }
    } while (continuous_mode && g_running);
    
    /* Cleanup */
    free_config(&g_config);
    
    printf("\npktscan finished\n");
    return 0;
}
