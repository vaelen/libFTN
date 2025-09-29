/*
 * fnmailer.h - FidoNet Mailer daemon header
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

#ifndef FTN_FNMAILER_H
#define FTN_FNMAILER_H

#include "ftn.h"
#include <signal.h>
#include <time.h>

/* Global signal flags - using same pattern as fntosser.c */
extern volatile sig_atomic_t fnmailer_shutdown_requested;
extern volatile sig_atomic_t fnmailer_reload_requested;
extern volatile sig_atomic_t fnmailer_dump_stats_requested;
extern volatile sig_atomic_t fnmailer_toggle_debug_requested;

/* Network context for each configured network */
typedef struct {
    const ftn_network_config_t* config;
    time_t next_poll_time;
    time_t last_successful_poll;
    int consecutive_failures;
    ftn_net_connection_t* active_connection;
} ftn_network_context_t;

/* Main mailer context */
typedef struct {
    ftn_config_t* config;
    char* config_filename;
    ftn_network_context_t* networks;
    size_t network_count;

    /* Daemon settings */
    int daemon_mode;
    int verbose;
    int sleep_interval;
    char* pid_file;

    /* Runtime state */
    int running;
    time_t start_time;

    /* Statistics */
    uint32_t total_connections;
    uint32_t successful_connections;
    uint32_t failed_connections;
    uint64_t bytes_sent;
    uint64_t bytes_received;
} ftn_mailer_context_t;

/* Command line options */
typedef struct {
    char* config_file;
    int daemon_mode;
    int sleep_interval;
    int verbose;
    int show_help;
    int show_version;
} ftn_mailer_options_t;

/* Signal handling */
void ftn_mailer_setup_signals(void);
void ftn_mailer_cleanup_signals(void);
void ftn_mailer_check_signals(ftn_mailer_context_t* mailer);

/* Mailer context management */
ftn_mailer_context_t* ftn_mailer_context_new(void);
void ftn_mailer_context_free(ftn_mailer_context_t* ctx);
ftn_error_t ftn_mailer_context_init(ftn_mailer_context_t* ctx, const ftn_mailer_options_t* options);

/* Main application functions */
ftn_error_t ftn_mailer_parse_args(int argc, char* argv[], ftn_mailer_options_t* options);
void ftn_mailer_show_help(const char* program_name);
void ftn_mailer_show_version(void);

/* Daemon operations */
ftn_error_t ftn_mailer_daemonize(ftn_mailer_context_t* ctx);
ftn_error_t ftn_mailer_create_pid_file(ftn_mailer_context_t* ctx);
void ftn_mailer_remove_pid_file(ftn_mailer_context_t* ctx);

/* Main loop */
ftn_error_t ftn_mailer_run(ftn_mailer_context_t* ctx);
ftn_error_t ftn_mailer_single_shot(ftn_mailer_context_t* ctx);
ftn_error_t ftn_mailer_daemon_loop(ftn_mailer_context_t* ctx);

/* Network operations */
ftn_error_t ftn_mailer_init_networks(ftn_mailer_context_t* ctx);
ftn_error_t ftn_mailer_poll_networks(ftn_mailer_context_t* ctx);
time_t ftn_mailer_calculate_next_poll(ftn_mailer_context_t* ctx);

/* Statistics and monitoring */
void ftn_mailer_dump_statistics(ftn_mailer_context_t* ctx);
void ftn_mailer_update_stats(ftn_mailer_context_t* ctx, int success, size_t bytes_sent, size_t bytes_received);

/* Configuration management */
ftn_error_t ftn_mailer_reload_config(ftn_mailer_context_t* ctx);

#endif /* FTN_FNMAILER_H */