/*
 * ftntoss - FidoNet Technology Network Message Tosser
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "ftn.h"
#include "ftn/config.h"
#include "ftn/version.h"
#include "ftn/packet.h"
#include "ftn/router.h"
#include "ftn/storage.h"
#include "ftn/dupecheck.h"

static volatile sig_atomic_t shutdown_requested = 0;
static volatile sig_atomic_t reload_requested = 0;
static int verbose_mode = 0;

/* Processing error types */
typedef enum {
    FTN_PROCESS_SUCCESS,
    FTN_PROCESS_ERROR_PACKET_PARSE,
    FTN_PROCESS_ERROR_DUPLICATE,
    FTN_PROCESS_ERROR_ROUTING,
    FTN_PROCESS_ERROR_STORAGE,
    FTN_PROCESS_ERROR_FILE_MOVE,
    FTN_PROCESS_ERROR_DIRECTORY,
    FTN_PROCESS_ERROR_PERMISSION
} ftn_process_error_t;

/* Processing statistics */
typedef struct {
    size_t packets_processed;
    size_t messages_processed;
    size_t duplicates_found;
    size_t messages_stored;
    size_t messages_forwarded;
    size_t errors_encountered;
    time_t processing_start_time;
    time_t processing_end_time;
} ftn_processing_stats_t;

/* Function prototypes */
static ftn_error_t ensure_directories_exist(const ftn_network_config_t* network);
static ftn_error_t move_packet_to_processed(const char* packet_path, const char* processed_dir);
static ftn_error_t move_packet_to_bad(const char* packet_path, const char* bad_dir);
static ftn_error_t process_single_packet(const char* packet_path, const ftn_network_config_t* network,
                                        ftn_router_t* router, ftn_storage_t* storage, ftn_dupecheck_t* dupecheck,
                                        ftn_processing_stats_t* stats);
static ftn_error_t process_message(const ftn_message_t* msg, const ftn_network_config_t* network,
                                  ftn_router_t* router, ftn_storage_t* storage, ftn_dupecheck_t* dupecheck,
                                  ftn_processing_stats_t* stats);
static int process_network_inbox_enhanced(const ftn_network_config_t* network, ftn_router_t* router,
                                         ftn_storage_t* storage, ftn_dupecheck_t* dupecheck,
                                         ftn_processing_stats_t* stats);
static void print_processing_stats(const ftn_processing_stats_t* stats);
static void init_processing_stats(ftn_processing_stats_t* stats);

void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nFidoNet Technology Network Message Tosser\n\n");
    printf("Options:\n");
    printf("  -c, --config FILE     Configuration file path (required)\n");
    printf("  -d, --daemon          Run in continuous (daemon) mode\n");
    printf("  -s, --sleep SECONDS   Sleep interval for daemon mode (default: 60)\n");
    printf("  -v, --verbose         Enable verbose logging\n");
    printf("  -h, --help            Show this help message\n");
    printf("      --version         Show version information\n");
    printf("\nExamples:\n");
    printf("  %s -c /etc/ftntoss.ini                # Single-shot mode\n", program_name);
    printf("  %s -c /etc/ftntoss.ini -d             # Daemon mode\n", program_name);
    printf("  %s -c /etc/ftntoss.ini -d -s 30       # Daemon mode, 30s intervals\n", program_name);
}

void print_version(void) {
    printf("ftntoss (libFTN) %d.%d.%d\n", FTN_VERSION_MAJOR, FTN_VERSION_MINOR, FTN_VERSION_PATCH);
    printf("Copyright (c) 2025 Andrew C. Young\n");
    printf("This is free software; see the source for copying conditions.\n");
}

void log_info(const char* format, ...) {
    va_list args;
    time_t now;
    struct tm* tm_info;
    char timestamp[32];

    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("[%s] INFO: ", timestamp);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

void log_error(const char* format, ...) {
    va_list args;
    time_t now;
    struct tm* tm_info;
    char timestamp[32];

    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(stderr, "[%s] ERROR: ", timestamp);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void log_debug(const char* format, ...) {
    va_list args;
    time_t now;
    struct tm* tm_info;
    char timestamp[32];

    if (!verbose_mode) {
        return;
    }

    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("[%s] DEBUG: ", timestamp);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

void signal_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            shutdown_requested = 1;
            log_info("Shutdown signal received, exiting gracefully");
            break;
        case SIGHUP:
            reload_requested = 1;
            log_info("Reload signal received, will reload configuration");
            break;
        case SIGPIPE:
            break;
        default:
            log_debug("Received signal %d", sig);
            break;
    }
}

void setup_signal_handlers(void) {
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGPIPE, SIG_IGN);
}

int process_inbox(const ftn_config_t* config) {
    ftn_processing_stats_t stats;
    ftn_router_t* router = NULL;
    ftn_storage_t* storage = NULL;
    ftn_dupecheck_t* dupecheck = NULL;
    const ftn_network_config_t* network;
    int result = 0;
    size_t i;

    if (!config) {
        log_error("Invalid configuration provided to process_inbox");
        return -1;
    }

    init_processing_stats(&stats);
    log_info("Processing inbox for %lu configured networks", (unsigned long)config->network_count);

    /* Initialize storage first */
    storage = ftn_storage_new(config);
    if (!storage) {
        log_error("Failed to initialize storage");
        return -1;
    }

    if (ftn_storage_initialize(storage) != FTN_OK) {
        log_error("Failed to initialize storage");
        ftn_storage_free(storage);
        return -1;
    }

    /* Initialize duplicate checker - use first network's duplicate_db path */
    if (config->network_count > 0 && config->networks[0].duplicate_db) {
        dupecheck = ftn_dupecheck_new(config->networks[0].duplicate_db);
    } else {
        /* Use default path */
        dupecheck = ftn_dupecheck_new("dupecheck.db");
    }

    if (!dupecheck) {
        log_error("Failed to initialize duplicate checker");
        ftn_storage_free(storage);
        return -1;
    }

    if (ftn_dupecheck_load(dupecheck) != FTN_OK) {
        log_error("Failed to load duplicate database");
        result = -1;
        goto cleanup;
    }

    /* Initialize router with config and dupecheck */
    router = ftn_router_new(config, dupecheck);
    if (!router) {
        log_error("Failed to initialize router");
        result = -1;
        goto cleanup;
    }

    /* Process each configured network */
    for (i = 0; i < config->network_count; i++) {
        network = &config->networks[i];
        log_debug("Processing network: %s", network->name);

        if (process_network_inbox_enhanced(network, router, storage, dupecheck, &stats) != 0) {
            log_error("Error processing network: %s", network->name);
            result = -1;
            /* Continue processing other networks */
        }
    }

    stats.processing_end_time = time(NULL);
    print_processing_stats(&stats);

cleanup:
    if (dupecheck) ftn_dupecheck_free(dupecheck);
    if (storage) ftn_storage_free(storage);
    if (router) ftn_router_free(router);

    return result;
}


int run_single_shot(const ftn_config_t* config) {
    log_info("Running in single-shot mode");

    if (process_inbox(config) != 0) {
        log_error("Error processing inbox");
        return -1;
    }

    log_info("Single-shot processing completed");
    return 0;
}

int run_continuous(const ftn_config_t* config, int sleep_interval) {
    int i;

    log_info("Running in continuous mode (sleep interval: %d seconds)", sleep_interval);

    while (!shutdown_requested) {
        log_debug("Starting processing cycle");

        if (process_inbox(config) != 0) {
            log_error("Error processing inbox, continuing");
        }

        if (reload_requested) {
            log_info("Configuration reload requested (not implemented yet)");
            reload_requested = 0;
        }

        log_debug("Processing cycle complete, sleeping for %d seconds", sleep_interval);

        for (i = 0; i < sleep_interval && !shutdown_requested; i++) {
            sleep(1);
        }
    }

    log_info("Continuous mode shutting down");
    return 0;
}

/* Initialize processing statistics */
static void init_processing_stats(ftn_processing_stats_t* stats) {
    if (!stats) return;

    memset(stats, 0, sizeof(ftn_processing_stats_t));
    stats->processing_start_time = time(NULL);
}

/* Print processing statistics */
static void print_processing_stats(const ftn_processing_stats_t* stats) {
    double elapsed_time;

    if (!stats) return;

    elapsed_time = difftime(stats->processing_end_time, stats->processing_start_time);

    log_info("Processing Statistics:");
    log_info("  Packets processed: %lu", (unsigned long)stats->packets_processed);
    log_info("  Messages processed: %lu", (unsigned long)stats->messages_processed);
    log_info("  Duplicates found: %lu", (unsigned long)stats->duplicates_found);
    log_info("  Messages stored: %lu", (unsigned long)stats->messages_stored);
    log_info("  Messages forwarded: %lu", (unsigned long)stats->messages_forwarded);
    log_info("  Errors encountered: %lu", (unsigned long)stats->errors_encountered);
    log_info("  Processing time: %.2f seconds", elapsed_time);
}

/* Ensure required directories exist */
static ftn_error_t ensure_directories_exist(const ftn_network_config_t* network) {
    struct stat st;

    if (!network) {
        return FTN_ERROR_INVALID;
    }

    /* Create inbox directory */
    if (network->inbox) {
        if (stat(network->inbox, &st) != 0) {
            if (mkdir(network->inbox, 0755) != 0) {
                log_error("Failed to create inbox directory: %s", network->inbox);
                return FTN_ERROR_FILE;
            }
            log_debug("Created inbox directory: %s", network->inbox);
        }
    }

    /* Create outbox directory */
    if (network->outbox) {
        if (stat(network->outbox, &st) != 0) {
            if (mkdir(network->outbox, 0755) != 0) {
                log_error("Failed to create outbox directory: %s", network->outbox);
                return FTN_ERROR_FILE;
            }
            log_debug("Created outbox directory: %s", network->outbox);
        }
    }

    /* Create processed directory */
    if (network->processed) {
        if (stat(network->processed, &st) != 0) {
            if (mkdir(network->processed, 0755) != 0) {
                log_error("Failed to create processed directory: %s", network->processed);
                return FTN_ERROR_FILE;
            }
            log_debug("Created processed directory: %s", network->processed);
        }
    }

    /* Create bad directory */
    if (network->bad) {
        if (stat(network->bad, &st) != 0) {
            if (mkdir(network->bad, 0755) != 0) {
                log_error("Failed to create bad directory: %s", network->bad);
                return FTN_ERROR_FILE;
            }
            log_debug("Created bad directory: %s", network->bad);
        }
    }

    return FTN_OK;
}

/* Move packet to processed directory */
static ftn_error_t move_packet_to_processed(const char* packet_path, const char* processed_dir) {
    char dest_path[512];
    const char* filename;

    if (!packet_path || !processed_dir) {
        return FTN_ERROR_INVALID;
    }

    filename = strrchr(packet_path, '/');
    if (filename) {
        filename++; /* Skip the '/' */
    } else {
        filename = packet_path;
    }

    snprintf(dest_path, sizeof(dest_path), "%s/%s", processed_dir, filename);

    if (rename(packet_path, dest_path) != 0) {
        log_error("Failed to move packet %s to processed directory: %s", packet_path, strerror(errno));
        return FTN_ERROR_FILE;
    }

    log_debug("Moved packet to processed: %s -> %s", packet_path, dest_path);
    return FTN_OK;
}

/* Move packet to bad directory */
static ftn_error_t move_packet_to_bad(const char* packet_path, const char* bad_dir) {
    char dest_path[512];
    const char* filename;

    if (!packet_path || !bad_dir) {
        return FTN_ERROR_INVALID;
    }

    filename = strrchr(packet_path, '/');
    if (filename) {
        filename++; /* Skip the '/' */
    } else {
        filename = packet_path;
    }

    snprintf(dest_path, sizeof(dest_path), "%s/%s", bad_dir, filename);

    if (rename(packet_path, dest_path) != 0) {
        log_error("Failed to move packet %s to bad directory: %s", packet_path, strerror(errno));
        return FTN_ERROR_FILE;
    }

    log_debug("Moved packet to bad: %s -> %s", packet_path, dest_path);
    return FTN_OK;
}

/* Process a single message */
static ftn_error_t process_message(const ftn_message_t* msg, const ftn_network_config_t* network,
                                  ftn_router_t* router, ftn_storage_t* storage, ftn_dupecheck_t* dupecheck,
                                  ftn_processing_stats_t* stats) {
    ftn_routing_decision_t decision;
    ftn_error_t error;
    int is_duplicate;

    if (!msg || !network || !router || !storage || !dupecheck || !stats) {
        return FTN_ERROR_INVALID;
    }

    stats->messages_processed++;

    /* Check for duplicates */
    error = ftn_dupecheck_is_duplicate(dupecheck, msg, &is_duplicate);
    if (error != FTN_OK) {
        log_error("Duplicate check failed for message");
        stats->errors_encountered++;
        return FTN_ERROR_INVALID;
    }

    if (is_duplicate) {
        log_debug("Skipping duplicate message: %s", msg->msgid ? msg->msgid : "no-msgid");
        stats->duplicates_found++;
        return FTN_OK;
    }

    /* Add to duplicate database */
    error = ftn_dupecheck_add_message(dupecheck, msg);
    if (error != FTN_OK) {
        log_error("Failed to add message to duplicate database");
        /* Continue processing - this is not fatal */
    }

    /* Determine routing */
    error = ftn_router_route_message(router, msg, &decision);
    if (error != FTN_OK) {
        log_error("Routing failed for message");
        stats->errors_encountered++;
        return FTN_ERROR_INVALID;
    }

    /* Store message based on routing decision */
    switch (decision.action) {
        case FTN_ROUTE_LOCAL_MAIL:
            error = ftn_storage_store_mail(storage, msg, decision.destination_user, network->name);
            if (error == FTN_OK) {
                stats->messages_stored++;
                log_debug("Stored netmail for user: %s", decision.destination_user);
            } else {
                log_error("Failed to store netmail for user: %s", decision.destination_user);
                stats->errors_encountered++;
                return FTN_ERROR_INVALID;
            }
            break;

        case FTN_ROUTE_LOCAL_NEWS:
            error = ftn_storage_store_news(storage, msg, decision.destination_area, network->name);
            if (error == FTN_OK) {
                stats->messages_stored++;
                log_debug("Stored echomail for area: %s", decision.destination_area);
            } else {
                log_error("Failed to store echomail for area: %s", decision.destination_area);
                stats->errors_encountered++;
                return FTN_ERROR_INVALID;
            }
            break;

        case FTN_ROUTE_FORWARD:
            /* TODO: Implement forwarding queue */
            stats->messages_forwarded++;
            {
                char addr_str[64];
                ftn_address_to_string(&decision.forward_to, addr_str, sizeof(addr_str));
                log_debug("Message marked for forwarding to %s", addr_str);
            }
            break;

        case FTN_ROUTE_DROP:
            log_debug("Dropping message per routing rules: %s", msg->msgid ? msg->msgid : "no-msgid");
            break;

        default:
            log_error("Invalid routing action: %d", decision.action);
            stats->errors_encountered++;
            return FTN_ERROR_INVALID;
    }

    return FTN_OK;
}

/* Process a single packet file */
static ftn_error_t process_single_packet(const char* packet_path, const ftn_network_config_t* network,
                                        ftn_router_t* router, ftn_storage_t* storage, ftn_dupecheck_t* dupecheck,
                                        ftn_processing_stats_t* stats) {
    ftn_packet_t* packet = NULL;
    ftn_error_t error;
    size_t i;

    if (!packet_path || !network || !router || !storage || !dupecheck || !stats) {
        return FTN_ERROR_INVALID;
    }

    log_debug("Processing packet: %s", packet_path);

    /* Load packet */
    error = ftn_packet_load(packet_path, &packet);
    if (error != FTN_OK) {
        log_error("Failed to load packet: %s", packet_path);
        stats->errors_encountered++;

        /* Move to bad directory */
        if (network->bad) {
            move_packet_to_bad(packet_path, network->bad);
        }
        return FTN_ERROR_PARSE;
    }

    stats->packets_processed++;
    log_debug("Loaded packet with %lu messages", (unsigned long)packet->message_count);

    /* Process each message in the packet */
    for (i = 0; i < packet->message_count; i++) {
        error = process_message(packet->messages[i], network, router, storage, dupecheck, stats);
        if (error != FTN_OK) {
            log_error("Error processing message %lu in packet %s", (unsigned long)(i + 1), packet_path);
            /* Continue processing other messages */
        }
    }

    /* Move packet to processed directory */
    if (network->processed) {
        error = move_packet_to_processed(packet_path, network->processed);
        if (error != FTN_OK) {
            log_error("Failed to move processed packet: %s", packet_path);
            /* Not fatal - packet was processed successfully */
        }
    }

    ftn_packet_free(packet);
    return FTN_OK;
}

/* Process network inbox */
static int process_network_inbox_enhanced(const ftn_network_config_t* network, ftn_router_t* router,
                                         ftn_storage_t* storage, ftn_dupecheck_t* dupecheck,
                                         ftn_processing_stats_t* stats) {
    DIR* dir;
    struct dirent* entry;
    char packet_path[512];
    int result = 0;

    if (!network || !router || !storage || !dupecheck || !stats) {
        log_error("Invalid parameters to process_network_inbox_enhanced");
        return -1;
    }

    log_info("Processing inbox for network: %s", network->name);

    /* Ensure directories exist */
    if (ensure_directories_exist(network) != FTN_OK) {
        log_error("Failed to ensure directories exist for network: %s", network->name);
        return -1;
    }

    if (!network->inbox) {
        log_error("No inbox path configured for network: %s", network->name);
        return -1;
    }

    /* Open inbox directory */
    dir = opendir(network->inbox);
    if (!dir) {
        log_error("Failed to open inbox directory: %s", network->inbox);
        return -1;
    }

    /* Process each .pkt file */
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue; /* Skip hidden files and . .. */
        }

        /* Check if it's a packet file */
        if (strlen(entry->d_name) > 4 &&
            strcasecmp(entry->d_name + strlen(entry->d_name) - 4, ".pkt") == 0) {

            snprintf(packet_path, sizeof(packet_path), "%s/%s", network->inbox, entry->d_name);

            if (process_single_packet(packet_path, network, router, storage, dupecheck, stats) != FTN_OK) {
                log_error("Error processing packet: %s", packet_path);
                result = -1;
                /* Continue processing other packets */
            }
        }
    }

    closedir(dir);
    return result;
}

int main(int argc, char* argv[]) {
    const char* config_file = NULL;
    int daemon_mode = 0;
    int sleep_interval = 60;
    ftn_config_t* config = NULL;
    int result = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                config_file = argv[++i];
            } else {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--daemon") == 0) {
            daemon_mode = 1;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--sleep") == 0) {
            if (i + 1 < argc) {
                sleep_interval = atoi(argv[++i]);
                if (sleep_interval <= 0) {
                    fprintf(stderr, "Error: Invalid sleep interval: %s\n", argv[i]);
                    return 1;
                }
            } else {
                fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose_mode = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!config_file) {
        fprintf(stderr, "Error: Configuration file is required\n");
        print_usage(argv[0]);
        return 1;
    }

    log_info("FTN Tosser starting up");
    log_debug("Configuration file: %s", config_file);
    log_debug("Daemon mode: %s", daemon_mode ? "yes" : "no");
    log_debug("Verbose mode: %s", verbose_mode ? "yes" : "no");

    config = ftn_config_new();
    if (!config) {
        log_error("Failed to allocate configuration structure");
        return 1;
    }

    if (ftn_config_load(config, config_file) != FTN_OK) {
        log_error("Failed to load configuration from: %s", config_file);
        ftn_config_free(config);
        return 1;
    }

    if (ftn_config_validate(config) != FTN_OK) {
        log_error("Configuration validation failed");
        ftn_config_free(config);
        return 1;
    }

    log_info("Configuration loaded and validated successfully");

    setup_signal_handlers();

    if (daemon_mode) {
        result = run_continuous(config, sleep_interval);
    } else {
        result = run_single_shot(config);
    }

    ftn_config_free(config);

    log_info("FTN Tosser shutting down");
    return result;
}