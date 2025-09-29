/*
 * fnmailer.c - FidoNet Mailer main executable
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

#include "ftn.h"
#include "ftn/mailer.h"
#include "ftn/log.h"

int main(int argc, char* argv[]) {
    ftn_mailer_options_t options;
    ftn_mailer_context_t* ctx = NULL;
    ftn_error_t result;
    int exit_code = 0;

    /* Parse command line arguments */
    result = ftn_mailer_parse_args(argc, argv, &options);
    if (result != FTN_OK) {
        fprintf(stderr, "Error parsing command line arguments\n");
        ftn_mailer_show_help(argv[0]);
        return 1;
    }

    /* Handle help and version requests */
    if (options.show_help) {
        ftn_mailer_show_help(argv[0]);
        goto cleanup;
    }

    if (options.show_version) {
        ftn_mailer_show_version();
        goto cleanup;
    }

    /* Initialize network layer */
    result = ftn_net_init();
    if (result != FTN_OK) {
        fprintf(stderr, "Failed to initialize network layer: %d\n", result);
        exit_code = 1;
        goto cleanup;
    }

    /* Create mailer context */
    ctx = ftn_mailer_context_new();
    if (!ctx) {
        fprintf(stderr, "Failed to allocate mailer context\n");
        exit_code = 1;
        goto cleanup;
    }

    /* Initialize context */
    result = ftn_mailer_context_init(ctx, &options);
    if (result != FTN_OK) {
        fprintf(stderr, "Failed to initialize mailer context: %d\n", result);
        exit_code = 1;
        goto cleanup;
    }

    /* Initialize logging */
    if (ctx->config && ctx->config->logging) {
        ftn_log_init(ctx->config->logging);
        if (options.verbose) {
            ftn_log_set_level(FTN_LOG_DEBUG);
        }
    } else {
        /* Default logging setup */
        ftn_logging_config_t log_config = {0};
        log_config.level = options.verbose ? FTN_LOG_DEBUG : FTN_LOG_INFO;
        log_config.log_file = NULL;
        log_config.ident = "fnmailer";
        ftn_log_init(&log_config);
    }

    logf_info("FNMailer starting (version %s)", FTN_VERSION_STRING);

    /* Daemonize if requested */
    if (options.daemon_mode) {
        result = ftn_mailer_daemonize(ctx);
        if (result != FTN_OK) {
            logf_error("Failed to daemonize: %d", result);
            exit_code = 1;
            goto cleanup;
        }

        /* Create PID file after daemonizing */
        result = ftn_mailer_create_pid_file(ctx);
        if (result != FTN_OK) {
            logf_warning("Failed to create PID file: %d", result);
            /* Not a fatal error */
        }
    }

    /* Run the mailer */
    result = ftn_mailer_run(ctx);
    if (result != FTN_OK) {
        logf_error("Mailer execution failed: %d", result);
        exit_code = 1;
    }

    logf_info("FNMailer shutdown complete");

cleanup:
    /* Remove PID file */
    if (ctx) {
        ftn_mailer_remove_pid_file(ctx);
    }

    /* Cleanup mailer context */
    if (ctx) {
        ftn_mailer_context_free(ctx);
    }

    /* Cleanup options */
    if (options.config_file) {
        free(options.config_file);
    }

    /* Cleanup network layer */
    ftn_net_cleanup();

    /* Cleanup logging */
    ftn_log_cleanup();

    return exit_code;
}