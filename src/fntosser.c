/*
 * fntosser - FidoNet Technology Network Message Tosser
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
#include <fcntl.h>

#include "ftn.h"
#include "ftn/config.h"
#include "ftn/version.h"
#include "ftn/packet.h"
#include "ftn/router.h"
#include "ftn/storage.h"
#include "ftn/dupechk.h"
#include "ftn/log.h"

/* Global daemon state */
static volatile sig_atomic_t shutdown_requested = 0;
static volatile sig_atomic_t reload_requested = 0;
static volatile sig_atomic_t dump_stats_requested = 0;
static volatile sig_atomic_t toggle_debug_requested = 0;
static int verbose_mode = 0;
static int daemon_mode = 0;
static const char* config_file_path = NULL;
static ftn_config_t* global_config = NULL;

static ftn_log_level_t current_log_level = FTN_LOG_INFO;

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

/* Global statistics */
typedef struct {
    unsigned long packets_processed;
    unsigned long messages_processed;
    unsigned long duplicates_detected;
    unsigned long messages_stored;
    unsigned long messages_forwarded;
    unsigned long errors_total;
    time_t start_time;
    time_t last_cycle_time;
    double avg_cycle_time;
    unsigned long cycles_completed;
} ftn_global_stats_t;

static ftn_global_stats_t global_stats = {0};

/* Logging compatibility function */
static void ftn_log_init_compat(ftn_log_level_t level, const char* ident) {
    ftn_logging_config_t config = {0};
    config.level = level;
    config.ident = (char*)ident; /* Safe cast for compatibility */
    ftn_log_init(&config);
}
/* Use new cleanup function */

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
static int run_single_shot(void);
static int daemonize(void);
static int setup_daemon_environment(void);
static int run_daemon_loop(int sleep_interval);
static void reload_configuration(void);
static void setup_daemon_signals(void);
static void handle_sighup(int sig);
static void handle_sigterm(int sig);
static void handle_sigusr1(int sig);
static void handle_sigusr2(int sig);
static int write_pid_file(const char* pid_file);
static int remove_pid_file(const char* pid_file);
static void ftn_stats_init(void);
static void ftn_stats_update(const ftn_processing_stats_t* stats);
static void ftn_stats_dump(void);

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
    printf("  %s -c /etc/fntosser.ini                # Single-shot mode\n", program_name);
    printf("  %s -c /etc/fntosser.ini -d             # Daemon mode\n", program_name);
    printf("  %s -c /etc/fntosser.ini -d -s 30       # Daemon mode, 30s intervals\n", program_name);
}

void print_version(void) {
    printf("fntosser (libFTN) %d.%d.%d\n", FTN_VERSION_MAJOR, FTN_VERSION_MINOR, FTN_VERSION_PATCH);
    printf("Copyright (c) 2025 Andrew C. Young\n");
    printf("This is free software; see the source for copying conditions.\n");
}

/* Daemon mode functions */
static int daemonize(void) {
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        return -1; /* Fork failed */
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS); /* Success: let the parent terminate */
    }

    /* On success: The child process becomes session leader */
    if (setsid() < 0) {
        return -1;
    }

    /* Fork again, allowing the parent of this second child to terminate */
    signal(SIGHUP, SIG_IGN);
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root */
    if (chdir("/") < 0) {
        return -1;
    }

    /* Close all open file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* Redirect standard files to /dev/null */
    if (open("/dev/null", O_RDONLY) < 0) {
        return -1;
    }
    if (open("/dev/null", O_WRONLY) < 0) {
        return -1;
    }
    if (open("/dev/null", O_RDWR) < 0) {
        return -1;
    }

    return 0;
}

static int setup_daemon_environment(void) {
    if (daemonize() != 0) {
        log_critical("Failed to daemonize process");
        return -1;
    }
    return 0;
}

static int write_pid_file(const char* pid_file) {
    FILE* f;

    if (!pid_file) {
        return 0; /* Not an error if no pid file is configured */
    }

    f = fopen(pid_file, "w");
    if (!f) {
        logf_error("Failed to open PID file for writing: %s", pid_file);
        return -1;
    }

    fprintf(f, "%d\n", getpid());
    fclose(f);
    return 0;
}

static int remove_pid_file(const char* pid_file) {
    if (!pid_file) {
        return 0;
    }

    if (unlink(pid_file) != 0) {
        logf_warning("Failed to remove PID file: %s", pid_file);
        return -1;
    }
    return 0;
}

/* Statistics functions */
static void ftn_stats_init(void) {
    memset(&global_stats, 0, sizeof(ftn_global_stats_t));
    global_stats.start_time = time(NULL);
}

static void ftn_stats_update(const ftn_processing_stats_t* stats) {
    double total_time;
    if (!stats) return;

    global_stats.packets_processed += stats->packets_processed;
    global_stats.messages_processed += stats->messages_processed;
    global_stats.duplicates_detected += stats->duplicates_found;
    global_stats.messages_stored += stats->messages_stored;
    global_stats.messages_forwarded += stats->messages_forwarded;
    global_stats.errors_total += stats->errors_encountered;

    global_stats.last_cycle_time = time(NULL);
    global_stats.cycles_completed++;

    total_time = difftime(global_stats.last_cycle_time, global_stats.start_time);
    if (global_stats.cycles_completed > 0) {
        global_stats.avg_cycle_time = total_time / global_stats.cycles_completed;
    }
}

static void ftn_stats_dump(void) {
    time_t uptime = time(NULL) - global_stats.start_time;
    char uptime_str[128];

    snprintf(uptime_str, sizeof(uptime_str), "%ldd %ldh %ldm %lds",
             uptime / 86400, (uptime % 86400) / 3600,
             (uptime % 3600) / 60, uptime % 60);

    log_info("=== FTN Tosser Statistics ===");
    logf_info("Uptime: %s", uptime_str);
    logf_info("Packets Processed: %lu", global_stats.packets_processed);
    logf_info("Messages Processed: %lu", global_stats.messages_processed);
    logf_info("Duplicates Detected: %lu", global_stats.duplicates_detected);
    logf_info("Messages Stored: %lu", global_stats.messages_stored);
    logf_info("Messages Forwarded: %lu", global_stats.messages_forwarded);
    logf_info("Total Errors: %lu", global_stats.errors_total);
    logf_info("Processing Cycles: %lu", global_stats.cycles_completed);
    logf_info("Average Cycle Time: %.2f seconds", global_stats.avg_cycle_time);
}



/* Configuration reload */
static void reload_configuration(void) {
    ftn_config_t* new_config = NULL;

    logf_info("Reloading configuration from: %s", config_file_path);

    new_config = ftn_config_new();
    if (!new_config) {
        log_error("Failed to allocate memory for new configuration");
        return;
    }

    if (ftn_config_load(new_config, config_file_path) != FTN_OK) {
        log_error("Failed to reload configuration, keeping current config");
        ftn_config_free(new_config);
        return;
    }

    if (ftn_config_validate(new_config) != FTN_OK) {
        log_error("New configuration is invalid, keeping current config");
        ftn_config_free(new_config);
        return;
    }

    /* This is a simple swap. In a multi-threaded environment, this would need a lock. */
    ftn_config_free(global_config);
    global_config = new_config;

    /* Re-initialize logging with potentially new settings */
    if (global_config->logging) {
        ftn_log_level_t log_level = verbose_mode ? FTN_LOG_DEBUG : global_config->logging->level;
        ftn_log_init_compat(log_level, global_config->logging->ident);
    }

    log_info("Configuration reloaded successfully");
}


/* Signal handling for daemon mode */
static void handle_sigterm(int sig) {
    (void)sig;
    shutdown_requested = 1;
    log_info("Shutdown signal received, exiting gracefully");
}

static void handle_sighup(int sig) {
    (void)sig;
    reload_requested = 1;
    log_info("Reload signal received, will reload configuration");
}

static void handle_sigusr1(int sig) {
    (void)sig;
    dump_stats_requested = 1;
    log_info("Statistics dump requested");
}

static void handle_sigusr2(int sig) {
    (void)sig;
    toggle_debug_requested = 1;
    log_info("Debug mode toggle requested");
}

static void setup_daemon_signals(void) {
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigterm); /* Treat INT as TERM */
    signal(SIGHUP, handle_sighup);
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);
    signal(SIGPIPE, SIG_IGN); /* Ignore broken pipes */
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
    logf_info("Processing inbox for %lu configured networks", (unsigned long)config->network_count);

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
        logf_debug("Processing network: %s", network->name);

        if (process_network_inbox_enhanced(network, router, storage, dupecheck, &stats) != 0) {
            logf_error("Error processing network: %s", network->name);
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


int run_single_shot(void) {
    log_info("Running in single-shot mode");

    if (process_inbox(global_config) != 0) {
        log_error("Error processing inbox");
        return -1;
    }

    log_info("Single-shot processing completed");
    return 0;
}

static int run_daemon_loop(int sleep_interval) {
    int i;
    ftn_stats_init();

    while (!shutdown_requested) {
        ftn_processing_stats_t stats;
        init_processing_stats(&stats);

        log_debug("Starting processing cycle");

        if (process_inbox(global_config) != 0) {
            log_error("Error processing inbox, continuing");
        }

        /* TODO: Implement process_outbound(global_config) */

        stats.processing_end_time = time(NULL);
        ftn_stats_update(&stats);

        if (reload_requested) {
            reload_configuration();
            reload_requested = 0;
        }
        if (dump_stats_requested) {
            ftn_stats_dump();
            dump_stats_requested = 0;
        }
        if (toggle_debug_requested) {
            current_log_level = (current_log_level == FTN_LOG_DEBUG) ? FTN_LOG_INFO : FTN_LOG_DEBUG;
            logf_info("Log level changed to %s", current_log_level == FTN_LOG_DEBUG ? "DEBUG" : "INFO");
            toggle_debug_requested = 0;
        }

        logf_debug("Processing cycle complete, sleeping for %d seconds", sleep_interval);
        for (i = 0; i < sleep_interval && !shutdown_requested; i++) {
            sleep(1);
        }
    }

    log_info("Daemon loop shutting down");
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
    logf_info("  Packets processed: %lu", (unsigned long)stats->packets_processed);
    logf_info("  Messages processed: %lu", (unsigned long)stats->messages_processed);
    logf_info("  Duplicates found: %lu", (unsigned long)stats->duplicates_found);
    logf_info("  Messages stored: %lu", (unsigned long)stats->messages_stored);
    logf_info("  Messages forwarded: %lu", (unsigned long)stats->messages_forwarded);
    logf_info("  Errors encountered: %lu", (unsigned long)stats->errors_encountered);
    logf_info("  Processing time: %.2f seconds", elapsed_time);
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
                logf_error("Failed to create inbox directory: %s", network->inbox);
                return FTN_ERROR_FILE;
            }
            logf_debug("Created inbox directory: %s", network->inbox);
        }
    }

    /* Create outbox directory */
    if (network->outbox) {
        if (stat(network->outbox, &st) != 0) {
            if (mkdir(network->outbox, 0755) != 0) {
                logf_error("Failed to create outbox directory: %s", network->outbox);
                return FTN_ERROR_FILE;
            }
            logf_debug("Created outbox directory: %s", network->outbox);
        }
    }

    /* Create processed directory */
    if (network->processed) {
        if (stat(network->processed, &st) != 0) {
            if (mkdir(network->processed, 0755) != 0) {
                logf_error("Failed to create processed directory: %s", network->processed);
                return FTN_ERROR_FILE;
            }
            logf_debug("Created processed directory: %s", network->processed);
        }
    }

    /* Create bad directory */
    if (network->bad) {
        if (stat(network->bad, &st) != 0) {
            if (mkdir(network->bad, 0755) != 0) {
                logf_error("Failed to create bad directory: %s", network->bad);
                return FTN_ERROR_FILE;
            }
            logf_debug("Created bad directory: %s", network->bad);
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
        logf_error("Failed to move packet %s to processed directory: %s", packet_path, strerror(errno));
        return FTN_ERROR_FILE;
    }

    logf_debug("Moved packet to processed: %s -> %s", packet_path, dest_path);
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
        logf_error("Failed to move packet %s to bad directory: %s", packet_path, strerror(errno));
        return FTN_ERROR_FILE;
    }

    logf_debug("Moved packet to bad: %s -> %s", packet_path, dest_path);
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
        logf_debug("Skipping duplicate message: %s", msg->msgid ? msg->msgid : "no-msgid");
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
                logf_debug("Stored netmail for user: %s", decision.destination_user);
            } else {
                logf_error("Failed to store netmail for user: %s", decision.destination_user);
                stats->errors_encountered++;
                return FTN_ERROR_INVALID;
            }
            break;

        case FTN_ROUTE_LOCAL_NEWS:
            error = ftn_storage_store_news(storage, msg, decision.destination_area, network->name);
            if (error == FTN_OK) {
                stats->messages_stored++;
                logf_debug("Stored echomail for area: %s", decision.destination_area);
            } else {
                logf_error("Failed to store echomail for area: %s", decision.destination_area);
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
                logf_debug("Message marked for forwarding to %s", addr_str);
            }
            break;

        case FTN_ROUTE_DROP:
            logf_debug("Dropping message per routing rules: %s", msg->msgid ? msg->msgid : "no-msgid");
            break;

        default:
            logf_error("Invalid routing action: %d", decision.action);
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

    logf_debug("Processing packet: %s", packet_path);

    /* Load packet */
    error = ftn_packet_load(packet_path, &packet);
    if (error != FTN_OK) {
        logf_error("Failed to load packet: %s", packet_path);
        stats->errors_encountered++;

        /* Move to bad directory */
        if (network->bad) {
            move_packet_to_bad(packet_path, network->bad);
        }
        return FTN_ERROR_PARSE;
    }

    stats->packets_processed++;
    logf_debug("Loaded packet with %lu messages", (unsigned long)packet->message_count);

    /* Process each message in the packet */
    for (i = 0; i < packet->message_count; i++) {
        error = process_message(packet->messages[i], network, router, storage, dupecheck, stats);
        if (error != FTN_OK) {
            logf_error("Error processing message %lu in packet %s", (unsigned long)(i + 1), packet_path);
            /* Continue processing other messages */
        }
    }

    /* Move packet to processed directory */
    if (network->processed) {
        error = move_packet_to_processed(packet_path, network->processed);
        if (error != FTN_OK) {
            logf_error("Failed to move processed packet: %s", packet_path);
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

    logf_info("Processing inbox for network: %s", network->name);

    /* Ensure directories exist */
    if (ensure_directories_exist(network) != FTN_OK) {
        logf_error("Failed to ensure directories exist for network: %s", network->name);
        return -1;
    }

    if (!network->inbox) {
        logf_error("No inbox path configured for network: %s", network->name);
        return -1;
    }

    /* Open inbox directory */
    dir = opendir(network->inbox);
    if (!dir) {
        logf_error("Failed to open inbox directory: %s", network->inbox);
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
                logf_error("Error processing packet: %s", packet_path);
                result = -1;
                /* Continue processing other packets */
            }
        }
    }

    closedir(dir);
    return result;
}

int main(int argc, char* argv[]) {
    int sleep_interval = 60;
    int result = 0;
    int i;

    /* Parse command-line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) {
                config_file_path = argv[++i];
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

    if (!config_file_path) {
        fprintf(stderr, "Error: Configuration file is required\n");
        print_usage(argv[0]);
        return 1;
    }

    /* Initialize logging early */
    ftn_log_init_compat(verbose_mode ? FTN_LOG_DEBUG : FTN_LOG_INFO, "fntosser");

    log_info("FTN Tosser starting up");
    logf_debug("Configuration file: %s", config_file_path);
    logf_debug("Daemon mode: %s", daemon_mode ? "yes" : "no");
    logf_debug("Verbose mode: %s", verbose_mode ? "yes" : "no");

    /* Load configuration */
    global_config = ftn_config_new();
    if (!global_config) {
        log_critical("Failed to allocate configuration structure");
        return 1;
    }
    if (ftn_config_load(global_config, config_file_path) != FTN_OK) {
        logf_critical("Failed to load configuration from: %s", config_file_path);
        ftn_config_free(global_config);
        return 1;
    }
    if (ftn_config_validate(global_config) != FTN_OK) {
        log_critical("Configuration validation failed");
        ftn_config_free(global_config);
        return 1;
    }

    log_info("Configuration loaded and validated successfully");

    /* Re-initialize logging based on config file settings */
    if (global_config->logging) {
        ftn_log_level_t log_level = verbose_mode ? FTN_LOG_DEBUG : global_config->logging->level;
        ftn_log_init_compat(log_level, global_config->logging->ident);
    }

    /* Determine sleep interval, command-line overrides config */
    if (global_config->daemon && global_config->daemon->sleep_interval > 0) {
        if (sleep_interval == 60) { /* Default not changed by CLI */
            sleep_interval = global_config->daemon->sleep_interval;
        }
    }

    /* Daemonize if requested */
    if (daemon_mode) {
        const char* pid_file = NULL;
        if (global_config->daemon) {
            pid_file = global_config->daemon->pid_file;
        }

        if (setup_daemon_environment() != 0) {
            return 1; /* setup_daemon_environment logs errors */
        }
        if (write_pid_file(pid_file) != 0) {
            /* Non-fatal, but log it */
            log_error("Failed to write PID file, continuing...");
        }

        logf_info("Process daemonized. PID file: %s", pid_file ? pid_file : "none");
    }

    setup_daemon_signals();

    if (daemon_mode) {
        result = run_daemon_loop(sleep_interval);
    } else {
        result = run_single_shot();
    }

    /* Clean up */
    if (daemon_mode && global_config && global_config->daemon) {
        remove_pid_file(global_config->daemon->pid_file);
    }
    ftn_config_free(global_config);
    log_info("FTN Tosser shutting down");
    ftn_log_cleanup();

    return result;
}