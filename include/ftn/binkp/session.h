/*
 * binkp_session.h - Binkp session state machine for libFTN
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

#ifndef FTN_BINKP_SESSION_H
#define FTN_BINKP_SESSION_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "../binkp.h"
#include "commands.h"
#include "../net.h"
#include "../config.h"

/* Session states */
typedef enum {
    /* Originating side states */
    BINKP_STATE_S0_CONN_INIT,
    BINKP_STATE_S1_WAIT_CONN,
    BINKP_STATE_S2_SEND_PASSWD,
    BINKP_STATE_S3_WAIT_ADDR,
    BINKP_STATE_S4_AUTH_REMOTE,
    BINKP_STATE_S5_IF_SECURE,
    BINKP_STATE_S6_WAIT_OK,
    BINKP_STATE_S7_OPTS,

    /* Answering side states */
    BINKP_STATE_R0_WAIT_CONN,
    BINKP_STATE_R1_WAIT_ADDR,
    BINKP_STATE_R2_IS_PASSWD,
    BINKP_STATE_R3_WAIT_PWD,
    BINKP_STATE_R4_PWD_ACK,
    BINKP_STATE_R5_OPTS,

    /* Transfer states */
    BINKP_STATE_T0_TRANSFER,
    BINKP_STATE_DONE,
    BINKP_STATE_ERROR
} ftn_binkp_session_state_t;

/* File transfer context */
typedef struct {
    char* filename;
    size_t file_size;
    time_t timestamp;
    size_t offset;
    size_t bytes_transferred;
    FILE* file_handle;
    uint32_t crc32;
} ftn_binkp_file_transfer_t;

/* Session context */
typedef struct {
    ftn_binkp_session_state_t state;
    ftn_net_connection_t* connection;
    ftn_config_t* config;

    /* Session properties */
    int is_originator;
    int is_secure;
    int authenticated;

    /* Address information */
    char* local_addresses;
    char* remote_addresses;
    char* session_password;

    /* File transfer */
    ftn_binkp_file_transfer_t* current_file;

    /* Protocol options */
    int supports_compression;
    int supports_crc;
    int supports_nr_mode;

    /* Statistics */
    time_t session_start;
    size_t bytes_sent;
    size_t bytes_received;
    int files_sent;
    int files_received;

    /* Timeouts */
    int frame_timeout_ms;
    int session_timeout_ms;
} ftn_binkp_session_t;

/* Session management */
ftn_binkp_error_t ftn_binkp_session_init(ftn_binkp_session_t* session, ftn_net_connection_t* conn, ftn_config_t* config, int is_originator);
void ftn_binkp_session_free(ftn_binkp_session_t* session);

/* Session execution */
ftn_binkp_error_t ftn_binkp_session_run(ftn_binkp_session_t* session);
ftn_binkp_error_t ftn_binkp_session_step(ftn_binkp_session_t* session);

/* State machine handlers */
ftn_binkp_error_t ftn_binkp_handle_originator_state(ftn_binkp_session_t* session);
ftn_binkp_error_t ftn_binkp_handle_answerer_state(ftn_binkp_session_t* session);
ftn_binkp_error_t ftn_binkp_handle_transfer_state(ftn_binkp_session_t* session);

/* Frame processing */
ftn_binkp_error_t ftn_binkp_process_frame(ftn_binkp_session_t* session, const ftn_binkp_frame_t* frame);
ftn_binkp_error_t ftn_binkp_process_command(ftn_binkp_session_t* session, const ftn_binkp_command_frame_t* cmd);
ftn_binkp_error_t ftn_binkp_process_data(ftn_binkp_session_t* session, const ftn_binkp_frame_t* frame);

/* Session utilities */
ftn_binkp_error_t ftn_binkp_send_command(ftn_binkp_session_t* session, ftn_binkp_command_t cmd, const char* args);
ftn_binkp_error_t ftn_binkp_send_frame(ftn_binkp_session_t* session, const ftn_binkp_frame_t* frame);
ftn_binkp_error_t ftn_binkp_receive_frame(ftn_binkp_session_t* session, ftn_binkp_frame_t* frame);

/* File transfer operations */
ftn_binkp_error_t ftn_binkp_file_transfer_init(ftn_binkp_file_transfer_t* transfer);
void ftn_binkp_file_transfer_free(ftn_binkp_file_transfer_t* transfer);
ftn_binkp_error_t ftn_binkp_start_file_send(ftn_binkp_session_t* session, const char* filename);
ftn_binkp_error_t ftn_binkp_start_file_receive(ftn_binkp_session_t* session, const ftn_binkp_file_info_t* file_info);
ftn_binkp_error_t ftn_binkp_continue_file_transfer(ftn_binkp_session_t* session);

/* Utility functions */
const char* ftn_binkp_session_state_name(ftn_binkp_session_state_t state);
int ftn_binkp_session_is_complete(const ftn_binkp_session_t* session);
int ftn_binkp_session_has_error(const ftn_binkp_session_t* session);

#endif /* FTN_BINKP_SESSION_H */