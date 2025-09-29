#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "ftn/transfer.h"
#include "ftn/log.h"

ftn_bso_error_t ftn_file_transfer_init(ftn_file_transfer_t* transfer) {
    if (!transfer) {
        return BSO_ERROR_INVALID_PATH;
    }

    memset(transfer, 0, sizeof(ftn_file_transfer_t));
    transfer->state = TRANSFER_STATE_IDLE;
    return BSO_OK;
}

void ftn_file_transfer_free(ftn_file_transfer_t* transfer) {
    if (!transfer) {
        return;
    }

    if (transfer->filename) {
        free(transfer->filename);
        transfer->filename = NULL;
    }

    if (transfer->temp_filename) {
        free(transfer->temp_filename);
        transfer->temp_filename = NULL;
    }

    if (transfer->file_handle) {
        fclose(transfer->file_handle);
        transfer->file_handle = NULL;
    }

    memset(transfer, 0, sizeof(ftn_file_transfer_t));
}

ftn_bso_error_t ftn_file_transfer_setup_send(ftn_file_transfer_t* transfer, const char* filepath, ftn_ref_directive_t action) {
    struct stat st;
    const char* filename;

    if (!transfer || !filepath) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Get file stats */
    if (stat(filepath, &st) != 0) {
        logf_error("Cannot stat file %s: %s", filepath, strerror(errno));
        return BSO_ERROR_NOT_FOUND;
    }

    ftn_file_transfer_init(transfer);

    /* Extract filename from path */
    filename = strrchr(filepath, '/');
    if (filename) {
        filename++;
    } else {
        filename = filepath;
    }

    transfer->filename = malloc(strlen(filepath) + 1);
    if (!transfer->filename) {
        return BSO_ERROR_MEMORY;
    }
    strcpy(transfer->filename, filepath);

    transfer->total_size = st.st_size;
    transfer->timestamp = st.st_mtime;
    transfer->action = action;
    transfer->state = TRANSFER_STATE_IDLE;

    logf_debug("Setup send transfer for %s (%zu bytes, action=%s)",
                  filename, transfer->total_size, ftn_flow_directive_string(action));

    return BSO_OK;
}

ftn_bso_error_t ftn_file_transfer_setup_receive(ftn_file_transfer_t* transfer, const char* filename, size_t size, time_t timestamp) {
    ftn_bso_error_t result;

    if (!transfer || !filename) {
        return BSO_ERROR_INVALID_PATH;
    }

    ftn_file_transfer_init(transfer);

    transfer->filename = malloc(strlen(filename) + 1);
    if (!transfer->filename) {
        return BSO_ERROR_MEMORY;
    }
    strcpy(transfer->filename, filename);

    transfer->total_size = size;
    transfer->timestamp = timestamp;
    transfer->state = TRANSFER_STATE_IDLE;

    /* Create temporary filename */
    result = ftn_transfer_create_temp_filename(filename, &transfer->temp_filename);
    if (result != BSO_OK) {
        ftn_file_transfer_free(transfer);
        return result;
    }

    logf_debug("Setup receive transfer for %s (%zu bytes)", filename, size);
    return BSO_OK;
}

ftn_bso_error_t ftn_transfer_context_init(ftn_transfer_context_t* ctx) {
    if (!ctx) {
        return BSO_ERROR_INVALID_PATH;
    }

    memset(ctx, 0, sizeof(ftn_transfer_context_t));
    ctx->pending_capacity = 10;
    ctx->pending_files = malloc(ctx->pending_capacity * sizeof(ftn_file_transfer_t*));
    if (!ctx->pending_files) {
        return BSO_ERROR_MEMORY;
    }

    return BSO_OK;
}

void ftn_transfer_context_free(ftn_transfer_context_t* ctx) {
    size_t i;

    if (!ctx) {
        return;
    }

    if (ctx->pending_files) {
        for (i = 0; i < ctx->pending_count; i++) {
            if (ctx->pending_files[i]) {
                ftn_file_transfer_free(ctx->pending_files[i]);
                free(ctx->pending_files[i]);
            }
        }
        free(ctx->pending_files);
        ctx->pending_files = NULL;
    }

    if (ctx->current_send) {
        ftn_file_transfer_free(ctx->current_send);
        free(ctx->current_send);
        ctx->current_send = NULL;
    }

    if (ctx->current_recv) {
        ftn_file_transfer_free(ctx->current_recv);
        free(ctx->current_recv);
        ctx->current_recv = NULL;
    }

    memset(ctx, 0, sizeof(ftn_transfer_context_t));
}

ftn_bso_error_t ftn_transfer_add_file(ftn_transfer_context_t* ctx, const ftn_file_transfer_t* transfer) {
    ftn_file_transfer_t* new_transfer;

    if (!ctx || !transfer) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Expand capacity if needed */
    if (ctx->pending_count >= ctx->pending_capacity) {
        ftn_file_transfer_t** new_files;
        ctx->pending_capacity *= 2;
        new_files = realloc(ctx->pending_files, ctx->pending_capacity * sizeof(ftn_file_transfer_t*));
        if (!new_files) {
            return BSO_ERROR_MEMORY;
        }
        ctx->pending_files = new_files;
    }

    /* Allocate and copy transfer */
    new_transfer = malloc(sizeof(ftn_file_transfer_t));
    if (!new_transfer) {
        return BSO_ERROR_MEMORY;
    }

    memcpy(new_transfer, transfer, sizeof(ftn_file_transfer_t));

    /* Duplicate strings */
    if (transfer->filename) {
        new_transfer->filename = malloc(strlen(transfer->filename) + 1);
        if (new_transfer->filename) {
            strcpy(new_transfer->filename, transfer->filename);
        }
    }

    if (transfer->temp_filename) {
        new_transfer->temp_filename = malloc(strlen(transfer->temp_filename) + 1);
        if (new_transfer->temp_filename) {
            strcpy(new_transfer->temp_filename, transfer->temp_filename);
        }
    }

    /* Clear file handle - will be opened when needed */
    new_transfer->file_handle = NULL;

    ctx->pending_files[ctx->pending_count] = new_transfer;
    ctx->pending_count++;
    ctx->total_files++;

    logf_debug("Added file to transfer queue: %s", new_transfer->filename);
    return BSO_OK;
}

ftn_bso_error_t ftn_transfer_load_from_flow(ftn_transfer_context_t* ctx, const ftn_flow_file_t* flow) {
    size_t i;
    ftn_file_transfer_t transfer;
    ftn_bso_error_t result;

    if (!ctx || !flow) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Process each entry in the flow file */
    for (i = 0; i < flow->file_count; i++) {
        const ftn_reference_entry_t* entry = &flow->entries[i];

        /* Skip processed or skip entries */
        if (entry->processed || entry->directive == REF_DIRECTIVE_SKIP) {
            continue;
        }

        /* Validate file exists */
        if (ftn_flow_validate_file_exists(entry) != BSO_OK) {
            logf_warning("File not found, skipping: %s", entry->filepath);
            continue;
        }

        /* Setup transfer */
        result = ftn_file_transfer_setup_send(&transfer, entry->filepath, entry->directive);
        if (result == BSO_OK) {
            if (flow->type == FLOW_TYPE_NETMAIL) {
                transfer.is_netmail = 1;
            }

            result = ftn_transfer_add_file(ctx, &transfer);
            ftn_file_transfer_free(&transfer);

            if (result != BSO_OK) {
                return result;
            }
        }
    }

    logf_info("Loaded %zu files from flow %s", flow->file_count, flow->filename);
    return BSO_OK;
}

ftn_bso_error_t ftn_transfer_start_batch(ftn_transfer_context_t* ctx) {
    if (!ctx) {
        return BSO_ERROR_INVALID_PATH;
    }

    ctx->batch_complete = 0;
    ctx->completed_files = 0;

    logf_info("Starting transfer batch with %zu files", ctx->pending_count);
    return BSO_OK;
}

ftn_bso_error_t ftn_transfer_process_next(ftn_transfer_context_t* ctx) {
    ftn_bso_error_t result;

    if (!ctx) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Check if we have files to send and no current send */
    if (!ctx->current_send && ctx->pending_count > 0) {
        /* Move next pending file to current send */
        ctx->current_send = ctx->pending_files[0];

        /* Shift remaining files down */
        {
            size_t i;
            for (i = 1; i < ctx->pending_count; i++) {
                ctx->pending_files[i - 1] = ctx->pending_files[i];
            }
            ctx->pending_count--;
        }

        /* Start sending the file */
        result = ftn_transfer_send_file_header(ctx, ctx->current_send);
        if (result != BSO_OK) {
            logf_error("Failed to send file header: %s", ftn_bso_error_string(result));
            return result;
        }

        ctx->current_send->state = TRANSFER_STATE_SENDING;
        ctx->current_send->start_time = time(NULL);
    }

    /* Process current send if active */
    if (ctx->current_send && ctx->current_send->state == TRANSFER_STATE_SENDING) {
        result = ftn_transfer_send_file_data(ctx, ctx->current_send);
        if (result != BSO_OK) {
            return result;
        }
    }

    /* Check if batch is complete */
    if (!ctx->current_send && !ctx->current_recv && ctx->pending_count == 0) {
        ctx->batch_complete = 1;
    }

    return BSO_OK;
}

ftn_bso_error_t ftn_transfer_send_file_header(ftn_transfer_context_t* ctx, ftn_file_transfer_t* transfer) {
    const char* filename;

    if (!ctx || !transfer || !ctx->session) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Extract filename from path */
    filename = strrchr(transfer->filename, '/');
    if (filename) {
        filename++;
    } else {
        filename = transfer->filename;
    }

    /* Send M_FILE command via binkp session */
    /* This would integrate with the binkp session to send the M_FILE command */
    logf_info("Sending file header: %s (%zu bytes)", filename, transfer->total_size);

    /* Open file for reading */
    transfer->file_handle = fopen(transfer->filename, "rb");
    if (!transfer->file_handle) {
        logf_error("Cannot open file for sending: %s", transfer->filename);
        return BSO_ERROR_FILE_IO;
    }

    /* Seek to resume offset if resuming */
    if (transfer->resume_offset > 0) {
        if (fseek(transfer->file_handle, transfer->resume_offset, SEEK_SET) != 0) {
            fclose(transfer->file_handle);
            transfer->file_handle = NULL;
            return BSO_ERROR_FILE_IO;
        }
        transfer->transferred = transfer->resume_offset;
    }

    return BSO_OK;
}

ftn_bso_error_t ftn_transfer_send_file_data(ftn_transfer_context_t* ctx, ftn_file_transfer_t* transfer) {
    char buffer[FTN_TRANSFER_CHUNK_SIZE];
    size_t bytes_read;
    ftn_bso_error_t result;

    if (!ctx || !transfer || !transfer->file_handle) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Read chunk from file */
    result = ftn_transfer_read_chunk(transfer, buffer, &bytes_read);
    if (result != BSO_OK) {
        return result;
    }

    if (bytes_read == 0) {
        /* EOF reached, file transfer complete */
        fclose(transfer->file_handle);
        transfer->file_handle = NULL;
        transfer->state = TRANSFER_STATE_WAITING_ACK;

        logf_info("File sent: %s (%zu bytes)", transfer->filename, transfer->transferred);
        return BSO_OK;
    }

    /* Send data via binkp session */
    /* This would integrate with the binkp session to send data frames */
    transfer->transferred += bytes_read;

    return BSO_OK;
}

ftn_bso_error_t ftn_transfer_read_chunk(ftn_file_transfer_t* transfer, void* buffer, size_t* bytes_read) {
    size_t remaining;
    size_t to_read;

    if (!transfer || !buffer || !bytes_read || !transfer->file_handle) {
        return BSO_ERROR_INVALID_PATH;
    }

    remaining = transfer->total_size - transfer->transferred;
    to_read = (remaining < FTN_TRANSFER_CHUNK_SIZE) ? remaining : FTN_TRANSFER_CHUNK_SIZE;

    if (to_read == 0) {
        *bytes_read = 0;
        return BSO_OK;
    }

    *bytes_read = fread(buffer, 1, to_read, transfer->file_handle);
    if (*bytes_read != to_read && !feof(transfer->file_handle)) {
        logf_error("Error reading from file: %s", transfer->filename);
        return BSO_ERROR_FILE_IO;
    }

    return BSO_OK;
}

ftn_bso_error_t ftn_transfer_write_chunk(ftn_file_transfer_t* transfer, const void* buffer, size_t bytes_to_write) {
    size_t bytes_written;

    if (!transfer || !buffer || !transfer->file_handle) {
        return BSO_ERROR_INVALID_PATH;
    }

    bytes_written = fwrite(buffer, 1, bytes_to_write, transfer->file_handle);
    if (bytes_written != bytes_to_write) {
        logf_error("Error writing to file: %s", transfer->temp_filename ? transfer->temp_filename : transfer->filename);
        return BSO_ERROR_FILE_IO;
    }

    transfer->transferred += bytes_written;
    return BSO_OK;
}

ftn_bso_error_t ftn_transfer_receive_file_header(ftn_transfer_context_t* ctx, const char* filename, size_t size, time_t timestamp, size_t offset) {
    ftn_bso_error_t result;

    if (!ctx || !filename) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Clean up any existing receive transfer */
    if (ctx->current_recv) {
        ftn_file_transfer_free(ctx->current_recv);
        free(ctx->current_recv);
    }

    ctx->current_recv = malloc(sizeof(ftn_file_transfer_t));
    if (!ctx->current_recv) {
        return BSO_ERROR_MEMORY;
    }

    result = ftn_file_transfer_setup_receive(ctx->current_recv, filename, size, timestamp);
    if (result != BSO_OK) {
        free(ctx->current_recv);
        ctx->current_recv = NULL;
        return result;
    }

    ctx->current_recv->resume_offset = offset;
    ctx->current_recv->state = TRANSFER_STATE_RECEIVING;
    ctx->current_recv->start_time = time(NULL);

    /* Open temporary file for writing */
    ctx->current_recv->file_handle = fopen(ctx->current_recv->temp_filename, offset > 0 ? "ab" : "wb");
    if (!ctx->current_recv->file_handle) {
        logf_error("Cannot open temp file for receiving: %s", ctx->current_recv->temp_filename);
        ftn_file_transfer_free(ctx->current_recv);
        free(ctx->current_recv);
        ctx->current_recv = NULL;
        return BSO_ERROR_FILE_IO;
    }

    ctx->current_recv->transferred = offset;

    logf_info("Receiving file: %s (%zu bytes, offset=%zu)", filename, size, offset);
    return BSO_OK;
}

ftn_bso_error_t ftn_transfer_receive_file_data(ftn_transfer_context_t* ctx, const void* data, size_t len) {
    if (!ctx || !ctx->current_recv || !data) {
        return BSO_ERROR_INVALID_PATH;
    }

    return ftn_transfer_write_chunk(ctx->current_recv, data, len);
}

ftn_bso_error_t ftn_transfer_create_temp_filename(const char* final_filename, char** temp_filename) {
    const char* basename;
    char* result;
    size_t len;

    if (!final_filename || !temp_filename) {
        return BSO_ERROR_INVALID_PATH;
    }

    basename = strrchr(final_filename, '/');
    if (basename) {
        basename++;
    } else {
        basename = final_filename;
    }

    len = strlen(basename) + 10;
    result = malloc(len);
    if (!result) {
        return BSO_ERROR_MEMORY;
    }

    snprintf(result, len, "%s.tmp", basename);
    *temp_filename = result;
    return BSO_OK;
}

const char* ftn_transfer_state_string(ftn_transfer_state_t state) {
    switch (state) {
        case TRANSFER_STATE_IDLE:
            return "idle";
        case TRANSFER_STATE_SENDING:
            return "sending";
        case TRANSFER_STATE_RECEIVING:
            return "receiving";
        case TRANSFER_STATE_WAITING_ACK:
            return "waiting_ack";
        case TRANSFER_STATE_COMPLETED:
            return "completed";
        case TRANSFER_STATE_ERROR:
            return "error";
        default:
            return "unknown";
    }
}

int ftn_transfer_is_complete(const ftn_transfer_context_t* ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->batch_complete;
}

double ftn_transfer_get_progress(const ftn_file_transfer_t* transfer) {
    if (!transfer || transfer->total_size == 0) {
        return 0.0;
    }
    return (double)transfer->transferred / (double)transfer->total_size;
}

ftn_bso_error_t ftn_transfer_apply_action(ftn_file_transfer_t* transfer) {
    if (!transfer || !transfer->filename) {
        return BSO_ERROR_INVALID_PATH;
    }

    switch (transfer->action) {
        case REF_DIRECTIVE_DELETE:
            if (unlink(transfer->filename) != 0) {
                logf_warning("Failed to delete file %s: %s", transfer->filename, strerror(errno));
            } else {
                logf_debug("Deleted file: %s", transfer->filename);
            }
            break;

        case REF_DIRECTIVE_TRUNCATE:
            {
                FILE* file = fopen(transfer->filename, "w");
                if (file) {
                    fclose(file);
                    logf_debug("Truncated file: %s", transfer->filename);
                } else {
                    logf_warning("Failed to truncate file %s: %s", transfer->filename, strerror(errno));
                }
            }
            break;

        case REF_DIRECTIVE_NONE:
        case REF_DIRECTIVE_SEND:
        case REF_DIRECTIVE_SKIP:
            /* No action needed */
            break;
    }

    return BSO_OK;
}

ftn_bso_error_t ftn_transfer_stats_init(ftn_transfer_stats_t* stats) {
    if (!stats) {
        return BSO_ERROR_INVALID_PATH;
    }

    memset(stats, 0, sizeof(ftn_transfer_stats_t));
    stats->start_time = time(NULL);
    return BSO_OK;
}

ftn_bso_error_t ftn_transfer_update_stats(ftn_transfer_stats_t* stats, const ftn_file_transfer_t* transfer, int is_send) {
    if (!stats || !transfer) {
        return BSO_ERROR_INVALID_PATH;
    }

    if (is_send) {
        stats->files_sent++;
        stats->bytes_sent += transfer->transferred;
    } else {
        stats->files_received++;
        stats->bytes_received += transfer->transferred;
    }

    stats->end_time = time(NULL);
    return BSO_OK;
}

double ftn_transfer_get_throughput(const ftn_transfer_stats_t* stats) {
    time_t elapsed;

    if (!stats || stats->start_time == 0) {
        return 0.0;
    }

    elapsed = stats->end_time - stats->start_time;
    if (elapsed <= 0) {
        return 0.0;
    }

    return (double)(stats->bytes_sent + stats->bytes_received) / (double)elapsed;
}