/*
 * transfer.h - BSO File transfer engine for libFTN
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

#ifndef FTN_TRANSFER_H
#define FTN_TRANSFER_H

#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include "bso.h"
#include "flow.h"
#include "binkp_session.h"

/* Forward declarations */
struct ftn_address;
struct ftn_binkp_session;

/* Transfer states */
typedef enum {
    TRANSFER_STATE_IDLE,
    TRANSFER_STATE_SENDING,
    TRANSFER_STATE_RECEIVING,
    TRANSFER_STATE_WAITING_ACK,
    TRANSFER_STATE_COMPLETED,
    TRANSFER_STATE_ERROR
} ftn_transfer_state_t;

/* File transfer context */
typedef struct {
    char* filename;
    char* temp_filename;
    size_t total_size;
    size_t transferred;
    time_t timestamp;
    FILE* file_handle;
    ftn_ref_directive_t action;
    int is_netmail;
    ftn_transfer_state_t state;
    time_t start_time;
    int resume_offset;
} ftn_file_transfer_t;

/* Transfer batch context */
typedef struct {
    ftn_file_transfer_t** pending_files;
    size_t pending_count;
    size_t pending_capacity;
    ftn_file_transfer_t* current_send;
    ftn_file_transfer_t* current_recv;
    struct ftn_binkp_session* session;
    int batch_complete;
    size_t total_files;
    size_t completed_files;
} ftn_transfer_context_t;

/* Transfer statistics */
typedef struct {
    size_t files_sent;
    size_t files_received;
    size_t bytes_sent;
    size_t bytes_received;
    time_t start_time;
    time_t end_time;
    int errors;
} ftn_transfer_stats_t;

/* File transfer operations */
ftn_bso_error_t ftn_file_transfer_init(ftn_file_transfer_t* transfer);
void ftn_file_transfer_free(ftn_file_transfer_t* transfer);
ftn_bso_error_t ftn_file_transfer_setup_send(ftn_file_transfer_t* transfer, const char* filepath, ftn_ref_directive_t action);
ftn_bso_error_t ftn_file_transfer_setup_receive(ftn_file_transfer_t* transfer, const char* filename, size_t size, time_t timestamp);

/* Transfer context operations */
ftn_bso_error_t ftn_transfer_context_init(ftn_transfer_context_t* ctx);
void ftn_transfer_context_free(ftn_transfer_context_t* ctx);
ftn_bso_error_t ftn_transfer_context_set_session(ftn_transfer_context_t* ctx, struct ftn_binkp_session* session);

/* Transfer queue management */
ftn_bso_error_t ftn_transfer_add_file(ftn_transfer_context_t* ctx, const ftn_file_transfer_t* transfer);
ftn_bso_error_t ftn_transfer_load_from_flow(ftn_transfer_context_t* ctx, const ftn_flow_file_t* flow);
ftn_bso_error_t ftn_transfer_load_outbound(ftn_transfer_context_t* ctx, const char* outbound_path, const struct ftn_address* target_addr);

/* Transfer execution */
ftn_bso_error_t ftn_transfer_start_batch(ftn_transfer_context_t* ctx);
ftn_bso_error_t ftn_transfer_process_next(ftn_transfer_context_t* ctx);
ftn_bso_error_t ftn_transfer_complete_batch(ftn_transfer_context_t* ctx);

/* File transmission */
ftn_bso_error_t ftn_transfer_send_file_header(ftn_transfer_context_t* ctx, ftn_file_transfer_t* transfer);
ftn_bso_error_t ftn_transfer_send_file_data(ftn_transfer_context_t* ctx, ftn_file_transfer_t* transfer);
ftn_bso_error_t ftn_transfer_handle_send_ack(ftn_transfer_context_t* ctx, const char* response);

/* File reception */
ftn_bso_error_t ftn_transfer_receive_file_header(ftn_transfer_context_t* ctx, const char* filename, size_t size, time_t timestamp, size_t offset);
ftn_bso_error_t ftn_transfer_receive_file_data(ftn_transfer_context_t* ctx, const void* data, size_t len);
ftn_bso_error_t ftn_transfer_complete_receive(ftn_transfer_context_t* ctx);

/* File validation and post-processing */
ftn_bso_error_t ftn_transfer_validate_received_file(const ftn_file_transfer_t* transfer);
ftn_bso_error_t ftn_transfer_apply_action(ftn_file_transfer_t* transfer);
ftn_bso_error_t ftn_transfer_move_to_final_location(ftn_file_transfer_t* transfer, const char* dest_dir);

/* Resume support */
ftn_bso_error_t ftn_transfer_check_resume(ftn_file_transfer_t* transfer, const char* partial_file);
ftn_bso_error_t ftn_transfer_create_temp_filename(const char* final_filename, char** temp_filename);

/* Transfer utilities */
const char* ftn_transfer_state_string(ftn_transfer_state_t state);
int ftn_transfer_is_complete(const ftn_transfer_context_t* ctx);
int ftn_transfer_has_error(const ftn_transfer_context_t* ctx);
double ftn_transfer_get_progress(const ftn_file_transfer_t* transfer);

/* Statistics */
ftn_bso_error_t ftn_transfer_stats_init(ftn_transfer_stats_t* stats);
ftn_bso_error_t ftn_transfer_update_stats(ftn_transfer_stats_t* stats, const ftn_file_transfer_t* transfer, int is_send);
double ftn_transfer_get_throughput(const ftn_transfer_stats_t* stats);

/* Error handling and recovery */
ftn_bso_error_t ftn_transfer_handle_error(ftn_transfer_context_t* ctx, ftn_bso_error_t error);
ftn_bso_error_t ftn_transfer_retry_file(ftn_transfer_context_t* ctx, ftn_file_transfer_t* transfer);
ftn_bso_error_t ftn_transfer_skip_file(ftn_transfer_context_t* ctx, ftn_file_transfer_t* transfer, const char* reason);

/* Integration with binkp protocol */
ftn_bso_error_t ftn_transfer_integrate_binkp_session(ftn_transfer_context_t* ctx, struct ftn_binkp_session* session);
ftn_bso_error_t ftn_transfer_handle_m_file(ftn_transfer_context_t* ctx, const char* filename, size_t size, time_t timestamp, size_t offset);
ftn_bso_error_t ftn_transfer_handle_m_got(ftn_transfer_context_t* ctx, const char* filename, size_t bytes_received);
ftn_bso_error_t ftn_transfer_handle_m_get(ftn_transfer_context_t* ctx, const char* filename, size_t offset);
ftn_bso_error_t ftn_transfer_handle_m_skip(ftn_transfer_context_t* ctx, const char* filename, size_t offset);

/* File chunk processing */
#define FTN_TRANSFER_CHUNK_SIZE 8192

ftn_bso_error_t ftn_transfer_read_chunk(ftn_file_transfer_t* transfer, void* buffer, size_t* bytes_read);
ftn_bso_error_t ftn_transfer_write_chunk(ftn_file_transfer_t* transfer, const void* buffer, size_t bytes_to_write);

#endif /* FTN_TRANSFER_H */