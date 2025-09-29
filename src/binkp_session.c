/*
 * binkp_session.c - BinkP protocol session management and state machine
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "ftn/binkp_session.h"
#include "ftn/log.h"

ftn_binkp_error_t ftn_binkp_session_init(ftn_binkp_session_t* session, ftn_net_connection_t* conn, ftn_config_t* config, int is_originator) {
    if (!session || !conn || !config) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    memset(session, 0, sizeof(ftn_binkp_session_t));

    session->connection = conn;
    session->config = config;
    session->is_originator = is_originator;
    session->session_start = time(NULL);

    /* Set timeouts */
    session->frame_timeout_ms = 30000;
    session->session_timeout_ms = 300000;

    /* Set initial state */
    if (is_originator) {
        session->state = BINKP_STATE_S0_CONN_INIT;
    } else {
        session->state = BINKP_STATE_R0_WAIT_CONN;
    }

    /* Build local address list */
    if (config->networks && config->network_count > 0 && config->networks[0].address_str) {
        session->local_addresses = malloc(strlen(config->networks[0].address_str) + 1);
        if (session->local_addresses) {
            strcpy(session->local_addresses, config->networks[0].address_str);
        }
    }

    logf_info("Initialized binkp session as %s", is_originator ? "originator" : "answerer");
    return BINKP_OK;
}

void ftn_binkp_session_free(ftn_binkp_session_t* session) {
    if (!session) {
        return;
    }

    if (session->local_addresses) {
        free(session->local_addresses);
        session->local_addresses = NULL;
    }

    if (session->remote_addresses) {
        free(session->remote_addresses);
        session->remote_addresses = NULL;
    }

    if (session->session_password) {
        free(session->session_password);
        session->session_password = NULL;
    }

    if (session->current_file) {
        ftn_binkp_file_transfer_free(session->current_file);
        free(session->current_file);
        session->current_file = NULL;
    }

    memset(session, 0, sizeof(ftn_binkp_session_t));
}

ftn_binkp_error_t ftn_binkp_session_run(ftn_binkp_session_t* session) {
    ftn_binkp_error_t result;
    time_t start_time;
    time_t current_time;

    if (!session) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    start_time = time(NULL);

    while (!ftn_binkp_session_is_complete(session) && !ftn_binkp_session_has_error(session)) {
        current_time = time(NULL);

        /* Check session timeout */
        if ((current_time - start_time) > (session->session_timeout_ms / 1000)) {
            logf_error("Binkp session timeout");
            session->state = BINKP_STATE_ERROR;
            return BINKP_ERROR_TIMEOUT;
        }

        result = ftn_binkp_session_step(session);
        if (result != BINKP_OK) {
            logf_error("Binkp session step failed: %s", ftn_binkp_error_string(result));
            session->state = BINKP_STATE_ERROR;
            return result;
        }
    }

    if (ftn_binkp_session_has_error(session)) {
        return BINKP_ERROR_PROTOCOL_ERROR;
    }

    logf_info("Binkp session completed successfully");
    return BINKP_OK;
}

ftn_binkp_error_t ftn_binkp_session_step(ftn_binkp_session_t* session) {
    if (!session) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    logf_debug("Processing state %s", ftn_binkp_session_state_name(session->state));

    /* Handle transfer state */
    if (session->state == BINKP_STATE_T0_TRANSFER) {
        return ftn_binkp_handle_transfer_state(session);
    }

    /* Handle originator states */
    if (session->is_originator) {
        return ftn_binkp_handle_originator_state(session);
    }

    /* Handle answerer states */
    return ftn_binkp_handle_answerer_state(session);
}

ftn_binkp_error_t ftn_binkp_handle_originator_state(ftn_binkp_session_t* session) {
    ftn_binkp_frame_t frame;
    ftn_binkp_error_t result;

    switch (session->state) {
        case BINKP_STATE_S0_CONN_INIT:
            /* Connection already established, move to next state */
            session->state = BINKP_STATE_S1_WAIT_CONN;
            return BINKP_OK;

        case BINKP_STATE_S1_WAIT_CONN:
            /* Send M_NUL with system info */
            result = ftn_binkp_send_command(session, BINKP_M_NUL, "libftn binkp/1.0");
            if (result != BINKP_OK) return result;

            /* Send M_ADR with our addresses */
            result = ftn_binkp_send_command(session, BINKP_M_ADR, session->local_addresses);
            if (result != BINKP_OK) return result;

            session->state = BINKP_STATE_S2_SEND_PASSWD;
            return BINKP_OK;

        case BINKP_STATE_S2_SEND_PASSWD:
            /* Send password if configured */
            if (session->config->networks && session->config->network_count > 0 && session->config->networks[0].password) {
                result = ftn_binkp_send_command(session, BINKP_M_PWD, session->config->networks[0].password);
                if (result != BINKP_OK) return result;
            }
            session->state = BINKP_STATE_S3_WAIT_ADDR;
            return BINKP_OK;

        case BINKP_STATE_S3_WAIT_ADDR:
            /* Wait for remote M_ADR */
            result = ftn_binkp_receive_frame(session, &frame);
            if (result != BINKP_OK) return result;

            result = ftn_binkp_process_frame(session, &frame);
            ftn_binkp_frame_free(&frame);
            return result;

        case BINKP_STATE_S4_AUTH_REMOTE:
            /* Address received, move to security check */
            session->state = BINKP_STATE_S5_IF_SECURE;
            return BINKP_OK;

        case BINKP_STATE_S5_IF_SECURE:
            /* Check if session is secure */
            if (session->is_secure) {
                session->state = BINKP_STATE_S6_WAIT_OK;
            } else {
                session->state = BINKP_STATE_S7_OPTS;
            }
            return BINKP_OK;

        case BINKP_STATE_S6_WAIT_OK:
            /* Wait for M_OK */
            result = ftn_binkp_receive_frame(session, &frame);
            if (result != BINKP_OK) return result;

            result = ftn_binkp_process_frame(session, &frame);
            ftn_binkp_frame_free(&frame);
            return result;

        case BINKP_STATE_S7_OPTS:
            /* Protocol options negotiation */
            session->state = BINKP_STATE_T0_TRANSFER;
            return BINKP_OK;

        default:
            logf_error("Unknown originator state: %d", session->state);
            session->state = BINKP_STATE_ERROR;
            return BINKP_ERROR_PROTOCOL_ERROR;
    }
}

ftn_binkp_error_t ftn_binkp_handle_answerer_state(ftn_binkp_session_t* session) {
    ftn_binkp_frame_t frame;
    ftn_binkp_error_t result;

    switch (session->state) {
        case BINKP_STATE_R0_WAIT_CONN:
            /* Send our address immediately */
            result = ftn_binkp_send_command(session, BINKP_M_ADR, session->local_addresses);
            if (result != BINKP_OK) return result;

            session->state = BINKP_STATE_R1_WAIT_ADDR;
            return BINKP_OK;

        case BINKP_STATE_R1_WAIT_ADDR:
            /* Wait for remote address */
            result = ftn_binkp_receive_frame(session, &frame);
            if (result != BINKP_OK) return result;

            result = ftn_binkp_process_frame(session, &frame);
            ftn_binkp_frame_free(&frame);
            return result;

        case BINKP_STATE_R2_IS_PASSWD:
            /* Check if password is required */
            if (session->config->networks && session->config->network_count > 0 && session->config->networks[0].password) {
                session->state = BINKP_STATE_R3_WAIT_PWD;
            } else {
                session->state = BINKP_STATE_R4_PWD_ACK;
            }
            return BINKP_OK;

        case BINKP_STATE_R3_WAIT_PWD:
            /* Wait for password */
            result = ftn_binkp_receive_frame(session, &frame);
            if (result != BINKP_OK) return result;

            result = ftn_binkp_process_frame(session, &frame);
            ftn_binkp_frame_free(&frame);
            return result;

        case BINKP_STATE_R4_PWD_ACK:
            /* Send M_OK to acknowledge authentication */
            result = ftn_binkp_send_command(session, BINKP_M_OK, "");
            if (result != BINKP_OK) return result;

            session->state = BINKP_STATE_R5_OPTS;
            return BINKP_OK;

        case BINKP_STATE_R5_OPTS:
            /* Protocol options negotiation */
            session->state = BINKP_STATE_T0_TRANSFER;
            return BINKP_OK;

        default:
            logf_error("Unknown answerer state: %d", session->state);
            session->state = BINKP_STATE_ERROR;
            return BINKP_ERROR_PROTOCOL_ERROR;
    }
}

ftn_binkp_error_t ftn_binkp_handle_transfer_state(ftn_binkp_session_t* session) {
    ftn_binkp_frame_t frame;
    ftn_binkp_error_t result;

    /* In transfer state, we process incoming frames and handle file transfers */
    result = ftn_binkp_receive_frame(session, &frame);
    if (result == BINKP_ERROR_TIMEOUT) {
        /* Timeout is acceptable in transfer state */
        return BINKP_OK;
    }
    if (result != BINKP_OK) {
        return result;
    }

    result = ftn_binkp_process_frame(session, &frame);
    ftn_binkp_frame_free(&frame);

    /* Check if session should end */
    if (result == BINKP_OK && session->state == BINKP_STATE_T0_TRANSFER) {
        /* For now, end the session after a short time in transfer state */
        session->state = BINKP_STATE_DONE;
    }

    return result;
}

ftn_binkp_error_t ftn_binkp_process_frame(ftn_binkp_session_t* session, const ftn_binkp_frame_t* frame) {
    ftn_binkp_command_frame_t cmd_frame;
    ftn_binkp_error_t result;

    if (!session || !frame) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    if (frame->is_command) {
        result = ftn_binkp_command_parse(frame, &cmd_frame);
        if (result != BINKP_OK) {
            return result;
        }

        result = ftn_binkp_process_command(session, &cmd_frame);
        ftn_binkp_command_free(&cmd_frame);
        return result;
    } else {
        return ftn_binkp_process_data(session, frame);
    }
}

ftn_binkp_error_t ftn_binkp_process_command(ftn_binkp_session_t* session, const ftn_binkp_command_frame_t* cmd) {
    if (!session || !cmd) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    logf_debug("Processing command %s", ftn_binkp_command_name(cmd->cmd));

    switch (cmd->cmd) {
        case BINKP_M_NUL:
            /* Information message, just log it */
            logf_info("Remote info: %s", cmd->args ? cmd->args : "");
            return BINKP_OK;

        case BINKP_M_ADR:
            /* Address information */
            if (cmd->args) {
                if (session->remote_addresses) {
                    free(session->remote_addresses);
                }
                session->remote_addresses = malloc(strlen(cmd->args) + 1);
                if (session->remote_addresses) {
                    strcpy(session->remote_addresses, cmd->args);
                }
                logf_info("Remote addresses: %s", cmd->args);

                /* Transition states based on current state */
                if (session->state == BINKP_STATE_S3_WAIT_ADDR) {
                    session->state = BINKP_STATE_S4_AUTH_REMOTE;
                } else if (session->state == BINKP_STATE_R1_WAIT_ADDR) {
                    session->state = BINKP_STATE_R2_IS_PASSWD;
                }
            }
            return BINKP_OK;

        case BINKP_M_PWD:
            /* Password authentication */
            if (session->config->networks && session->config->network_count > 0 && session->config->networks[0].password && cmd->args) {
                if (strcmp(cmd->args, session->config->networks[0].password) == 0) {
                    session->authenticated = 1;
                    session->is_secure = 1;
                    logf_info("Authentication successful");
                } else {
                    logf_error("Authentication failed");
                    return ftn_binkp_send_command(session, BINKP_M_ERR, "Authentication failed");
                }
            }
            if (session->state == BINKP_STATE_R3_WAIT_PWD) {
                session->state = BINKP_STATE_R4_PWD_ACK;
            }
            return BINKP_OK;

        case BINKP_M_OK:
            /* Acknowledgment */
            logf_info("Received M_OK: %s", cmd->args ? cmd->args : "");
            if (session->state == BINKP_STATE_S6_WAIT_OK) {
                session->state = BINKP_STATE_S7_OPTS;
            }
            return BINKP_OK;

        case BINKP_M_EOB:
            /* End of batch */
            logf_info("End of batch received");
            session->state = BINKP_STATE_DONE;
            return BINKP_OK;

        case BINKP_M_ERR:
            /* Error message */
            logf_error("Remote error: %s", cmd->args ? cmd->args : "");
            session->state = BINKP_STATE_ERROR;
            return BINKP_ERROR_PROTOCOL_ERROR;

        case BINKP_M_BSY:
            /* Busy message */
            logf_warning("Remote busy: %s", cmd->args ? cmd->args : "");
            session->state = BINKP_STATE_ERROR;
            return BINKP_ERROR_PROTOCOL_ERROR;

        case BINKP_M_FILE:
        case BINKP_M_GOT:
        case BINKP_M_GET:
        case BINKP_M_SKIP:
            /* File transfer commands - not implemented yet */
            logf_debug("File transfer command %s received (not implemented)", ftn_binkp_command_name(cmd->cmd));
            return BINKP_OK;

        default:
            /* Unknown command, ignore for forward compatibility */
            logf_warning("Unknown command: %d", cmd->cmd);
            return BINKP_OK;
    }
}

ftn_binkp_error_t ftn_binkp_process_data(ftn_binkp_session_t* session, const ftn_binkp_frame_t* frame) {
    if (!session || !frame) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    /* Data frames are used for file transfer - not implemented yet */
    logf_debug("Received data frame of %zu bytes (file transfer not implemented)", frame->size);
    session->bytes_received += frame->size;

    return BINKP_OK;
}

ftn_binkp_error_t ftn_binkp_send_command(ftn_binkp_session_t* session, ftn_binkp_command_t cmd, const char* args) {
    ftn_binkp_command_frame_t cmd_frame;
    ftn_binkp_frame_t frame;
    ftn_binkp_error_t result;

    if (!session) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    ftn_binkp_command_init(&cmd_frame);
    result = ftn_binkp_command_create(&cmd_frame, cmd, args);
    if (result != BINKP_OK) {
        return result;
    }

    ftn_binkp_frame_init(&frame);
    result = ftn_binkp_command_to_frame(&cmd_frame, &frame);
    if (result == BINKP_OK) {
        result = ftn_binkp_send_frame(session, &frame);
    }

    ftn_binkp_command_free(&cmd_frame);
    ftn_binkp_frame_free(&frame);

    logf_debug("Sent command %s", ftn_binkp_command_name(cmd));
    return result;
}

ftn_binkp_error_t ftn_binkp_send_frame(ftn_binkp_session_t* session, const ftn_binkp_frame_t* frame) {
    if (!session || !frame) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    session->bytes_sent += ftn_binkp_frame_total_size(frame);
    return ftn_binkp_frame_send(session->connection, frame);
}

ftn_binkp_error_t ftn_binkp_receive_frame(ftn_binkp_session_t* session, ftn_binkp_frame_t* frame) {
    ftn_binkp_error_t result;

    if (!session || !frame) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    result = ftn_binkp_frame_receive(session->connection, frame, session->frame_timeout_ms);
    if (result == BINKP_OK) {
        session->bytes_received += ftn_binkp_frame_total_size(frame);
    }

    return result;
}

ftn_binkp_error_t ftn_binkp_file_transfer_init(ftn_binkp_file_transfer_t* transfer) {
    if (!transfer) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    memset(transfer, 0, sizeof(ftn_binkp_file_transfer_t));
    return BINKP_OK;
}

void ftn_binkp_file_transfer_free(ftn_binkp_file_transfer_t* transfer) {
    if (!transfer) {
        return;
    }

    if (transfer->filename) {
        free(transfer->filename);
        transfer->filename = NULL;
    }

    if (transfer->file_handle) {
        fclose(transfer->file_handle);
        transfer->file_handle = NULL;
    }

    memset(transfer, 0, sizeof(ftn_binkp_file_transfer_t));
}

const char* ftn_binkp_session_state_name(ftn_binkp_session_state_t state) {
    switch (state) {
        case BINKP_STATE_S0_CONN_INIT: return "S0_CONN_INIT";
        case BINKP_STATE_S1_WAIT_CONN: return "S1_WAIT_CONN";
        case BINKP_STATE_S2_SEND_PASSWD: return "S2_SEND_PASSWD";
        case BINKP_STATE_S3_WAIT_ADDR: return "S3_WAIT_ADDR";
        case BINKP_STATE_S4_AUTH_REMOTE: return "S4_AUTH_REMOTE";
        case BINKP_STATE_S5_IF_SECURE: return "S5_IF_SECURE";
        case BINKP_STATE_S6_WAIT_OK: return "S6_WAIT_OK";
        case BINKP_STATE_S7_OPTS: return "S7_OPTS";
        case BINKP_STATE_R0_WAIT_CONN: return "R0_WAIT_CONN";
        case BINKP_STATE_R1_WAIT_ADDR: return "R1_WAIT_ADDR";
        case BINKP_STATE_R2_IS_PASSWD: return "R2_IS_PASSWD";
        case BINKP_STATE_R3_WAIT_PWD: return "R3_WAIT_PWD";
        case BINKP_STATE_R4_PWD_ACK: return "R4_PWD_ACK";
        case BINKP_STATE_R5_OPTS: return "R5_OPTS";
        case BINKP_STATE_T0_TRANSFER: return "T0_TRANSFER";
        case BINKP_STATE_DONE: return "DONE";
        case BINKP_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

int ftn_binkp_session_is_complete(const ftn_binkp_session_t* session) {
    if (!session) {
        return 0;
    }
    return session->state == BINKP_STATE_DONE;
}

int ftn_binkp_session_has_error(const ftn_binkp_session_t* session) {
    if (!session) {
        return 1;
    }
    return session->state == BINKP_STATE_ERROR;
}