/*
 * fnmailer.c - FidoNet Mailer daemon implementation
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
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>

#include "ftn.h"
#include "ftn/fnmailer.h"
#include "ftn/log.h"
#include "ftn/version.h"

/* Global signal context for signal handlers */
static ftn_signal_context_t* g_signal_context = NULL;

/* Signal handlers */
static void signal_handler_shutdown(int sig) {
    char byte = 1;
    (void)sig; /* Suppress unused parameter warning */

    if (g_signal_context) {
        g_signal_context->shutdown_requested = 1;
        /* Write to self-pipe to wake up main loop */
        if (g_signal_context->signal_pipe[1] >= 0) {
            write(g_signal_context->signal_pipe[1], &byte, 1);
        }
    }
}

static void signal_handler_reload(int sig) {
    char byte = 2;
    (void)sig; /* Suppress unused parameter warning */

    if (g_signal_context) {
        g_signal_context->reload_config = 1;
        if (g_signal_context->signal_pipe[1] >= 0) {
            write(g_signal_context->signal_pipe[1], &byte, 1);
        }
    }
}

static void signal_handler_stats(int sig) {
    char byte = 3;
    (void)sig; /* Suppress unused parameter warning */

    if (g_signal_context) {
        g_signal_context->dump_stats = 1;
        if (g_signal_context->signal_pipe[1] >= 0) {
            write(g_signal_context->signal_pipe[1], &byte, 1);
        }
    }
}

static void signal_handler_debug(int sig) {
    char byte = 4;
    (void)sig; /* Suppress unused parameter warning */

    if (g_signal_context) {
        g_signal_context->toggle_debug = 1;
        if (g_signal_context->signal_pipe[1] >= 0) {
            write(g_signal_context->signal_pipe[1], &byte, 1);
        }
    }
}

/* Signal handling implementation */
ftn_error_t ftn_signal_init(ftn_signal_context_t* ctx) {
    int flags;

    if (!ctx) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    memset(ctx, 0, sizeof(ftn_signal_context_t));
    ctx->signal_pipe[0] = -1;
    ctx->signal_pipe[1] = -1;

    /* Create self-pipe for signal handling */
    if (pipe(ctx->signal_pipe) == -1) {
        return FTN_ERROR_NETWORK;
    }

    /* Make write end non-blocking */
    flags = fcntl(ctx->signal_pipe[1], F_GETFL);
    if (flags == -1 || fcntl(ctx->signal_pipe[1], F_SETFL, flags | O_NONBLOCK) == -1) {
        close(ctx->signal_pipe[0]);
        close(ctx->signal_pipe[1]);
        return FTN_ERROR_NETWORK;
    }

    /* Set global context for signal handlers */
    g_signal_context = ctx;

    /* Install signal handlers */
    signal(SIGTERM, signal_handler_shutdown);
    signal(SIGINT, signal_handler_shutdown);
    signal(SIGHUP, signal_handler_reload);
    signal(SIGUSR1, signal_handler_stats);
    signal(SIGUSR2, signal_handler_debug);

    /* Ignore SIGPIPE */
    signal(SIGPIPE, SIG_IGN);

    return FTN_OK;
}

void ftn_signal_cleanup(ftn_signal_context_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Restore default signal handlers */
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
    signal(SIGUSR1, SIG_DFL);
    signal(SIGUSR2, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);

    /* Close signal pipe */
    if (ctx->signal_pipe[0] >= 0) {
        close(ctx->signal_pipe[0]);
        ctx->signal_pipe[0] = -1;
    }
    if (ctx->signal_pipe[1] >= 0) {
        close(ctx->signal_pipe[1]);
        ctx->signal_pipe[1] = -1;
    }

    g_signal_context = NULL;
}

void ftn_signal_check(ftn_signal_context_t* ctx, ftn_mailer_context_t* mailer) {
    char buffer[256];
    ssize_t bytes_read;

    if (!ctx || ctx->signal_pipe[0] < 0) {
        return;
    }

    /* Drain the signal pipe */
    while ((bytes_read = read(ctx->signal_pipe[0], buffer, sizeof(buffer))) > 0) {
        /* Signal received, flags are already set by handlers */
    }

    /* Handle shutdown request */
    if (ctx->shutdown_requested) {
        if (mailer) {
            mailer->running = 0;
        }
        logf_info("Shutdown requested");
        ctx->shutdown_requested = 0;
    }

    /* Handle config reload */
    if (ctx->reload_config) {
        if (mailer) {
            ftn_mailer_reload_config(mailer);
        }
        logf_info("Configuration reloaded");
        ctx->reload_config = 0;
    }

    /* Handle statistics dump */
    if (ctx->dump_stats) {
        if (mailer) {
            ftn_mailer_dump_statistics(mailer);
        }
        ctx->dump_stats = 0;
    }

    /* Handle debug toggle */
    if (ctx->toggle_debug) {
        ftn_log_level_t current = ftn_log_get_level();
        if (current == FTN_LOG_DEBUG) {
            ftn_log_set_level(FTN_LOG_INFO);
            logf_info("Debug logging disabled");
        } else {
            ftn_log_set_level(FTN_LOG_DEBUG);
            logf_info("Debug logging enabled");
        }
        ctx->toggle_debug = 0;
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

    /* Cleanup signal handling */
    ftn_signal_cleanup(&ctx->signals);

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

    /* Initialize signal handling */
    result = ftn_signal_init(&ctx->signals);
    if (result != FTN_OK) {
        return result;
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
    fd_set read_fds;
    struct timeval timeout;
    int max_fd;
    time_t next_poll_time;
    time_t now;
    time_t sleep_time;
    int activity;

    if (!ctx) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    logf_info("Starting daemon mode");

    while (ctx->running) {
        /* Check for signals */
        ftn_signal_check(&ctx->signals, ctx);

        if (!ctx->running) {
            break;
        }

        /* Check if it's time to poll networks */
        now = time(NULL);
        next_poll_time = ftn_mailer_calculate_next_poll(ctx);

        if (now >= next_poll_time) {
            ftn_mailer_poll_networks(ctx);
            next_poll_time = ftn_mailer_calculate_next_poll(ctx);
        }

        /* Setup select for signal pipe and timeout */
        FD_ZERO(&read_fds);
        max_fd = ctx->signals.signal_pipe[0];
        FD_SET(ctx->signals.signal_pipe[0], &read_fds);

        /* Calculate timeout until next poll */
        sleep_time = next_poll_time - now;
        if (sleep_time <= 0) {
            sleep_time = 1;
        }

        timeout.tv_sec = sleep_time;
        timeout.tv_usec = 0;

        /* Wait for activity or timeout */
        activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (activity > 0) {
            /* Signal received, will be handled at top of loop */
            continue;
        } else if (activity == 0) {
            /* Timeout - time to poll networks */
            continue;
        } else if (activity < 0 && errno != EINTR) {
            logf_error("select() failed: %s", strerror(errno));
            return FTN_ERROR_NETWORK;
        }
    }

    logf_info("Daemon mode shutdown");
    return FTN_OK;
}