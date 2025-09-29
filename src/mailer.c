/*
 * mailer.c - FidoNet Mailer daemon implementation
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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include "ftn.h"
#include "ftn/mailer.h"
#include "ftn/log.h"
#include "ftn/version.h"

/* Global signal flags - same pattern as fntosser.c */
volatile sig_atomic_t fnmailer_shutdown_requested = 0;
volatile sig_atomic_t fnmailer_reload_requested = 0;
volatile sig_atomic_t fnmailer_dump_stats_requested = 0;
volatile sig_atomic_t fnmailer_toggle_debug_requested = 0;

/* Signal handlers - same pattern as fntosser.c */
static void fnmailer_handle_sigterm(int sig) {
    (void)sig;
    fnmailer_shutdown_requested = 1;
    logf_info("Shutdown signal received, exiting gracefully");
}

static void fnmailer_handle_sighup(int sig) {
    (void)sig;
    fnmailer_reload_requested = 1;
    logf_info("Reload signal received, will reload configuration");
}

static void fnmailer_handle_sigusr1(int sig) {
    (void)sig;
    fnmailer_dump_stats_requested = 1;
    logf_info("Statistics dump requested");
}

static void fnmailer_handle_sigusr2(int sig) {
    (void)sig;
    fnmailer_toggle_debug_requested = 1;
    logf_info("Debug mode toggle requested");
}

/* Signal handling implementation - same pattern as fntosser.c */
void ftn_mailer_setup_signals(void) {
    signal(SIGTERM, fnmailer_handle_sigterm);
    signal(SIGINT, fnmailer_handle_sigterm); /* Treat INT as TERM */
    signal(SIGHUP, fnmailer_handle_sighup);
    signal(SIGUSR1, fnmailer_handle_sigusr1);
    signal(SIGUSR2, fnmailer_handle_sigusr2);
    signal(SIGPIPE, SIG_IGN); /* Ignore broken pipes */
}

void ftn_mailer_cleanup_signals(void) {
    /* Restore default signal handlers */
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
    signal(SIGUSR1, SIG_DFL);
    signal(SIGUSR2, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
}

void ftn_mailer_check_signals(ftn_mailer_context_t* mailer) {
    /* Handle shutdown request */
    if (fnmailer_shutdown_requested) {
        if (mailer) {
            mailer->running = 0;
        }
        logf_info("Shutdown requested");
        fnmailer_shutdown_requested = 0;
    }

    /* Handle config reload */
    if (fnmailer_reload_requested) {
        if (mailer) {
            ftn_mailer_reload_config(mailer);
        }
        logf_info("Configuration reloaded");
        fnmailer_reload_requested = 0;
    }

    /* Handle statistics dump */
    if (fnmailer_dump_stats_requested) {
        if (mailer) {
            ftn_mailer_dump_statistics(mailer);
        }
        fnmailer_dump_stats_requested = 0;
    }

    /* Handle debug toggle */
    if (fnmailer_toggle_debug_requested) {
        ftn_log_level_t current = ftn_log_get_level();
        if (current == FTN_LOG_DEBUG) {
            ftn_log_set_level(FTN_LOG_INFO);
            logf_info("Debug logging disabled");
        } else {
            ftn_log_set_level(FTN_LOG_DEBUG);
            logf_info("Debug logging enabled");
        }
        fnmailer_toggle_debug_requested = 0;
    }
}

/* Mailer context management */
ftn_mailer_context_t* ftn_mailer_context_new(void) {
    ftn_mailer_context_t* ctx = malloc(sizeof(ftn_mailer_context_t));
    if (!ctx) {
        return NULL;
    }

    memset(ctx, 0, sizeof(ftn_mailer_context_t));
    ctx->start_time = time(NULL);
    return ctx;
}

void ftn_mailer_context_free(ftn_mailer_context_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Free config */
    if (ctx->config) {
        ftn_config_free(ctx->config);
    }

    if (ctx->config_filename) {
        free(ctx->config_filename);
    }

    /* Free network contexts */
    if (ctx->networks) {
        size_t i;
        for (i = 0; i < ctx->network_count; i++) {
            if (ctx->networks[i].active_connection) {
                ftn_net_connection_free(ctx->networks[i].active_connection);
            }
        }
        free(ctx->networks);
    }

    if (ctx->pid_file) {
        free(ctx->pid_file);
    }

    free(ctx);
}

ftn_error_t ftn_mailer_context_init(ftn_mailer_context_t* ctx, const ftn_mailer_options_t* options) {
    ftn_error_t result;

    if (!ctx || !options) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Copy options */
    ctx->daemon_mode = options->daemon_mode;
    ctx->verbose = options->verbose;
    ctx->sleep_interval = options->sleep_interval;

    if (options->config_file) {
        ctx->config_filename = malloc(strlen(options->config_file) + 1);
        if (!ctx->config_filename) {
            return FTN_ERROR_NOMEM;
        }
        strcpy(ctx->config_filename, options->config_file);
    }

    /* Load configuration */
    ctx->config = ftn_config_new();
    if (!ctx->config) {
        return FTN_ERROR_NOMEM;
    }

    result = ftn_config_load(ctx->config, ctx->config_filename);
    if (result != FTN_OK) {
        return result;
    }

    result = ftn_config_validate_mailer(ctx->config);
    if (result != FTN_OK) {
        return result;
    }

    /* Setup PID file path */
    if (ctx->config->daemon && ctx->config->daemon->pid_file) {
        ctx->pid_file = malloc(strlen(ctx->config->daemon->pid_file) + 1);
        if (!ctx->pid_file) {
            return FTN_ERROR_NOMEM;
        }
        strcpy(ctx->pid_file, ctx->config->daemon->pid_file);
    }

    /* Initialize network contexts */
    result = ftn_mailer_init_networks(ctx);
    if (result != FTN_OK) {
        return result;
    }

    ctx->running = 1;
    return FTN_OK;
}

/* Command line parsing */
ftn_error_t ftn_mailer_parse_args(int argc, char* argv[], ftn_mailer_options_t* options) {
    int c;
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"daemon", no_argument, 0, 'd'},
        {"sleep", required_argument, 0, 's'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    if (!options) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    memset(options, 0, sizeof(ftn_mailer_options_t));
    options->sleep_interval = 60; /* Default 60 seconds */

    while ((c = getopt_long(argc, argv, "c:ds:vh", long_options, NULL)) != -1) {
        switch (c) {
            case 'c':
                if (options->config_file) {
                    free(options->config_file);
                }
                options->config_file = malloc(strlen(optarg) + 1);
                if (!options->config_file) {
                    return FTN_ERROR_NOMEM;
                }
                strcpy(options->config_file, optarg);
                break;

            case 'd':
                options->daemon_mode = 1;
                break;

            case 's':
                options->sleep_interval = atoi(optarg);
                if (options->sleep_interval <= 0) {
                    options->sleep_interval = 60;
                }
                break;

            case 'v':
                options->verbose = 1;
                break;

            case 'h':
                options->show_help = 1;
                break;

            case 0:
                /* Long options */
                if (strcmp(long_options[optind-1].name, "version") == 0) {
                    options->show_version = 1;
                }
                break;

            case '?':
                /* Unknown option */
                return FTN_ERROR_INVALID_PARAMETER;

            default:
                return FTN_ERROR_INVALID_PARAMETER;
        }
    }

    /* Config file is required */
    if (!options->show_help && !options->show_version && !options->config_file) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    return FTN_OK;
}

void ftn_mailer_show_help(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("\n");
    printf("Options:\n");
    printf("  -c, --config FILE     Configuration file path (required)\n");
    printf("  -d, --daemon          Run in continuous (daemon) mode\n");
    printf("  -s, --sleep SECONDS   Sleep interval for daemon mode (default: 60)\n");
    printf("  -v, --verbose         Enable verbose logging\n");
    printf("  -h, --help            Show this help message\n");
    printf("      --version         Show version information\n");
    printf("\n");
    printf("Signals (daemon mode):\n");
    printf("  SIGTERM/SIGINT        Graceful shutdown\n");
    printf("  SIGHUP                Reload configuration\n");
    printf("  SIGUSR1               Dump statistics\n");
    printf("  SIGUSR2               Toggle debug logging\n");
}

void ftn_mailer_show_version(void) {
    printf("fnmailer %s\n", FTN_VERSION_STRING);
    printf("FidoNet Mailer - TCP/IP binkp protocol implementation\n");
    printf("Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>\n");
    printf("This is free software; see the source for copying conditions.\n");
}

/* Daemon operations */
ftn_error_t ftn_mailer_daemonize(ftn_mailer_context_t* ctx) {
    pid_t pid;

    if (!ctx) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Fork first child */
    pid = fork();
    if (pid < 0) {
        return FTN_ERROR_NETWORK;
    }

    if (pid > 0) {
        /* Parent process exits */
        exit(0);
    }

    /* First child continues */
    /* Create new session */
    if (setsid() < 0) {
        return FTN_ERROR_NETWORK;
    }

    /* Fork second child */
    pid = fork();
    if (pid < 0) {
        return FTN_ERROR_NETWORK;
    }

    if (pid > 0) {
        /* First child exits */
        exit(0);
    }

    /* Second child continues as daemon */
    /* Change working directory to root */
    if (chdir("/") < 0) {
        return FTN_ERROR_NETWORK;
    }

    /* Close standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* Redirect to /dev/null */
    open("/dev/null", O_RDONLY); /* stdin */
    open("/dev/null", O_WRONLY); /* stdout */
    open("/dev/null", O_WRONLY); /* stderr */

    return FTN_OK;
}

ftn_error_t ftn_mailer_create_pid_file(ftn_mailer_context_t* ctx) {
    FILE* fp;
    pid_t pid;

    if (!ctx || !ctx->pid_file) {
        return FTN_OK; /* PID file is optional */
    }

    pid = getpid();

    fp = fopen(ctx->pid_file, "w");
    if (!fp) {
        return FTN_ERROR_FILE_ACCESS;
    }

    fprintf(fp, "%d\n", (int)pid);
    fclose(fp);

    return FTN_OK;
}

void ftn_mailer_remove_pid_file(ftn_mailer_context_t* ctx) {
    if (ctx && ctx->pid_file) {
        unlink(ctx->pid_file);
    }
}

/* Network operations */
ftn_error_t ftn_mailer_init_networks(ftn_mailer_context_t* ctx) {
    size_t i;

    if (!ctx || !ctx->config) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    ctx->network_count = ctx->config->network_count;
    if (ctx->network_count == 0) {
        return FTN_ERROR_INVALID;
    }

    ctx->networks = malloc(ctx->network_count * sizeof(ftn_network_context_t));
    if (!ctx->networks) {
        return FTN_ERROR_NOMEM;
    }

    memset(ctx->networks, 0, ctx->network_count * sizeof(ftn_network_context_t));

    for (i = 0; i < ctx->network_count; i++) {
        ctx->networks[i].config = &ctx->config->networks[i];
        ctx->networks[i].next_poll_time = ctx->start_time; /* Poll immediately on startup */
        ctx->networks[i].last_successful_poll = 0;
        ctx->networks[i].consecutive_failures = 0;
        ctx->networks[i].active_connection = NULL;
    }

    return FTN_OK;
}

ftn_error_t ftn_mailer_poll_networks(ftn_mailer_context_t* ctx) {
    size_t i;
    time_t now = time(NULL);

    if (!ctx) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    for (i = 0; i < ctx->network_count; i++) {
        ftn_network_context_t* net = &ctx->networks[i];

        /* Check if polling is due */
        if (now < net->next_poll_time) {
            continue;
        }

        logf_debug("Polling network %s", net->config->section_name);

        /* Simple connection test for now - this will be expanded in later tasks */
        if (net->config->hub_hostname) {
            ftn_net_connection_t* conn = ftn_net_connect(net->config->hub_hostname,
                                                       net->config->hub_port, 5000);
            if (conn) {
                logf_info("Successfully connected to %s:%d",
                           net->config->hub_hostname, net->config->hub_port);

                /* Update statistics */
                ctx->successful_connections++;
                net->last_successful_poll = now;
                net->consecutive_failures = 0;

                /* Close connection for now - actual protocol will be implemented later */
                ftn_net_connection_free(conn);
            } else {
                logf_warning("Failed to connect to %s:%d",
                              net->config->hub_hostname, net->config->hub_port);

                ctx->failed_connections++;
                net->consecutive_failures++;
            }

            ctx->total_connections++;
        }

        /* Schedule next poll */
        net->next_poll_time = now + net->config->poll_frequency;
    }

    return FTN_OK;
}

time_t ftn_mailer_calculate_next_poll(ftn_mailer_context_t* ctx) {
    time_t next_poll = 0;
    size_t i;

    if (!ctx) {
        return time(NULL) + 300; /* Default 5 minutes */
    }

    for (i = 0; i < ctx->network_count; i++) {
        time_t network_next = ctx->networks[i].next_poll_time;
        if (next_poll == 0 || network_next < next_poll) {
            next_poll = network_next;
        }
    }

    return next_poll;
}

/* Statistics and monitoring */
void ftn_mailer_dump_statistics(ftn_mailer_context_t* ctx) {
    time_t uptime;
    size_t i;

    if (!ctx) {
        return;
    }

    uptime = time(NULL) - ctx->start_time;

    logf_info("=== FNMailer Statistics ===");
    logf_info("Uptime: %ld seconds", uptime);
    logf_info("Connections: %u total, %u successful, %u failed",
                ctx->total_connections, ctx->successful_connections, ctx->failed_connections);
    logf_info("Data: %lu bytes sent, %lu bytes received",
                (unsigned long)ctx->bytes_sent, (unsigned long)ctx->bytes_received);

    logf_info("=== Network Status ===");
    for (i = 0; i < ctx->network_count; i++) {
        ftn_network_context_t* net = &ctx->networks[i];
        logf_info("Network %s: last_poll=%ld, next_poll=%ld, failures=%d",
                    net->config->section_name ? net->config->section_name : "unknown",
                    net->last_successful_poll, net->next_poll_time, net->consecutive_failures);
    }
}

void ftn_mailer_update_stats(ftn_mailer_context_t* ctx, int success, size_t bytes_sent, size_t bytes_received) {
    if (!ctx) {
        return;
    }

    if (success) {
        ctx->successful_connections++;
    } else {
        ctx->failed_connections++;
    }

    ctx->total_connections++;
    ctx->bytes_sent += bytes_sent;
    ctx->bytes_received += bytes_received;
}

/* Configuration management */
ftn_error_t ftn_mailer_reload_config(ftn_mailer_context_t* ctx) {
    ftn_error_t result;

    if (!ctx || !ctx->config_filename) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    logf_info("Reloading configuration from %s", ctx->config_filename);

    result = ftn_config_reload(ctx->config, ctx->config_filename);
    if (result != FTN_OK) {
        logf_error("Failed to reload configuration: %d", result);
        return result;
    }

    result = ftn_config_validate_mailer(ctx->config);
    if (result != FTN_OK) {
        logf_error("Configuration validation failed: %d", result);
        return result;
    }

    /* Reinitialize networks if needed */
    result = ftn_mailer_init_networks(ctx);
    if (result != FTN_OK) {
        logf_error("Failed to reinitialize networks: %d", result);
        return result;
    }

    logf_info("Configuration reloaded successfully");
    return FTN_OK;
}

/* Main loop implementations */
ftn_error_t ftn_mailer_run(ftn_mailer_context_t* ctx) {
    if (!ctx) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    if (ctx->daemon_mode) {
        return ftn_mailer_daemon_loop(ctx);
    } else {
        return ftn_mailer_single_shot(ctx);
    }
}

ftn_error_t ftn_mailer_single_shot(ftn_mailer_context_t* ctx) {
    ftn_error_t result;

    if (!ctx) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    logf_info("Starting single-shot mode");

    /* Poll all networks once */
    result = ftn_mailer_poll_networks(ctx);
    if (result != FTN_OK) {
        logf_error("Network polling failed: %d", result);
        return result;
    }

    logf_info("Single-shot mode completed");
    return FTN_OK;
}

ftn_error_t ftn_mailer_daemon_loop(ftn_mailer_context_t* ctx) {
    time_t next_poll_time;
    time_t now;
    int i;

    if (!ctx) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    logf_info("Starting daemon mode");

    /* Setup signal handlers */
    ftn_mailer_setup_signals();

    while (ctx->running && !fnmailer_shutdown_requested) {
        logf_debug("Starting processing cycle");

        /* Check for signals */
        ftn_mailer_check_signals(ctx);

        if (!ctx->running || fnmailer_shutdown_requested) {
            break;
        }

        /* Check if it's time to poll networks */
        now = time(NULL);
        next_poll_time = ftn_mailer_calculate_next_poll(ctx);

        if (now >= next_poll_time) {
            ftn_mailer_poll_networks(ctx);
        }

        logf_debug("Processing cycle complete, sleeping for %d seconds", ctx->sleep_interval);

        /* Sleep with interruption check - same pattern as fntosser.c */
        for (i = 0; i < ctx->sleep_interval && !fnmailer_shutdown_requested; i++) {
            sleep(1);
        }
    }

    logf_info("Daemon mode shutdown");
    return FTN_OK;
}