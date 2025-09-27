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

#include "ftn.h"
#include "ftn/config.h"
#include "ftn/version.h"

static volatile sig_atomic_t shutdown_requested = 0;
static volatile sig_atomic_t reload_requested = 0;
static int verbose_mode = 0;

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
    log_debug("Processing inbox (placeholder implementation)");

    if (!config) {
        log_error("Invalid configuration provided to process_inbox");
        return -1;
    }

    log_info("Processing inbox for configured networks");

    return 0;
}

int process_network_inbox(const ftn_network_config_t* network) {
    log_debug("Processing network inbox for %s (placeholder implementation)",
              network ? network->name : "unknown");

    if (!network) {
        log_error("Invalid network configuration provided");
        return -1;
    }

    log_info("Processing inbox for network: %s", network->name);

    return 0;
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