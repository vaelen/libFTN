#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ftn/binkp_crc.h"
#include "ftn/log.h"

/* CRC32 polynomial (IEEE 802.3) */
#define CRC32_POLYNOMIAL 0xEDB88320UL

/* Global CRC32 lookup table */
static uint32_t crc32_table[256];
static int crc32_table_initialized = 0;

void ftn_crc32_init_table(void) {
    uint32_t crc;
    int i, j;

    if (crc32_table_initialized) {
        return;
    }

    for (i = 0; i < 256; i++) {
        crc = (uint32_t)i;
        for (j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }

    crc32_table_initialized = 1;
}

uint32_t ftn_crc32_calculate(const uint8_t* data, size_t len) {
    return ftn_crc32_update(0xFFFFFFFFUL, data, len) ^ 0xFFFFFFFFUL;
}

uint32_t ftn_crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    size_t i;

    if (!crc32_table_initialized) {
        ftn_crc32_init_table();
    }

    if (!data) {
        return crc;
    }

    for (i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc;
}

ftn_binkp_error_t ftn_crc32_file(const char* filename, uint32_t* crc) {
    FILE* file;
    uint8_t buffer[4096];
    size_t bytes_read;
    uint32_t file_crc;

    if (!filename || !crc) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    file = fopen(filename, "rb");
    if (!file) {
        logf_error("Failed to open file %s for CRC calculation", filename);
        return BINKP_ERROR_PROTOCOL_ERROR;
    }

    file_crc = 0xFFFFFFFFUL;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        file_crc = ftn_crc32_update(file_crc, buffer, bytes_read);
    }

    fclose(file);

    *crc = file_crc ^ 0xFFFFFFFFUL;
    return BINKP_OK;
}

ftn_binkp_error_t ftn_crc_init(ftn_crc_context_t* ctx) {
    if (!ctx) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    memset(ctx, 0, sizeof(ftn_crc_context_t));
    ctx->local_mode = CRC_MODE_NONE;
    ctx->remote_mode = CRC_MODE_NONE;
    ctx->algorithm = CRC_ALGORITHM_NONE;

    /* Initialize CRC table if not already done */
    ftn_crc32_init_table();

    return BINKP_OK;
}

void ftn_crc_free(ftn_crc_context_t* ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->current_filename) {
        free(ctx->current_filename);
        ctx->current_filename = NULL;
    }

    memset(ctx, 0, sizeof(ftn_crc_context_t));
}

ftn_binkp_error_t ftn_crc_set_mode(ftn_crc_context_t* ctx, ftn_crc_mode_t mode) {
    if (!ctx) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    ctx->local_mode = mode;
    ctx->crc_enabled = (mode != CRC_MODE_NONE);

    if (ctx->crc_enabled) {
        ctx->algorithm = CRC_ALGORITHM_CRC32;
    }

    logf_debug("Set CRC mode to %s", ftn_crc_mode_name(mode));
    return BINKP_OK;
}

ftn_binkp_error_t ftn_crc_negotiate(ftn_crc_context_t* ctx, const char* remote_option) {
    ftn_crc_mode_t remote_mode;
    ftn_crc_algorithm_t remote_algorithm;
    ftn_binkp_error_t result;

    if (!ctx || !remote_option) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* Parse remote CRC option */
    result = ftn_crc_parse_option(remote_option, &remote_mode, &remote_algorithm);
    if (result != BINKP_OK) {
        return result;
    }

    ctx->remote_mode = remote_mode;

    /* Negotiate CRC mode */
    if (ctx->local_mode == CRC_MODE_REQUIRED) {
        if (remote_mode == CRC_MODE_NONE) {
            logf_error("CRC mode required but remote does not support it");
            return BINKP_ERROR_AUTH_FAILED;
        }
        ctx->crc_negotiated = 1;
        ctx->algorithm = remote_algorithm;
    } else if (ctx->local_mode == CRC_MODE_SUPPORTED) {
        if (remote_mode != CRC_MODE_NONE) {
            ctx->crc_negotiated = 1;
            ctx->algorithm = remote_algorithm;
        }
    } else {
        /* Local mode is NONE */
        if (remote_mode == CRC_MODE_REQUIRED) {
            logf_error("Remote requires CRC mode but local does not support it");
            return BINKP_ERROR_AUTH_FAILED;
        }
        ctx->crc_negotiated = 0;
    }

    logf_info("CRC mode negotiation: local=%s, remote=%s, negotiated=%s, algorithm=%s",
                 ftn_crc_mode_name(ctx->local_mode),
                 ftn_crc_mode_name(ctx->remote_mode),
                 ctx->crc_negotiated ? "yes" : "no",
                 ftn_crc_algorithm_name(ctx->algorithm));

    return BINKP_OK;
}

ftn_binkp_error_t ftn_crc_create_option(const ftn_crc_context_t* ctx, char** option) {
    if (!ctx || !option) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    if (ctx->local_mode == CRC_MODE_NONE) {
        *option = NULL;
        return BINKP_OK;
    }

    /* Create CRC option string */
    *option = malloc(16);
    if (!*option) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    snprintf(*option, 16, "CRC");
    return BINKP_OK;
}

ftn_binkp_error_t ftn_crc_start_file(ftn_crc_context_t* ctx, const char* filename, uint32_t expected_crc) {
    if (!ctx || !filename) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* Free existing filename */
    if (ctx->current_filename) {
        free(ctx->current_filename);
    }

    ctx->current_filename = malloc(strlen(filename) + 1);
    if (!ctx->current_filename) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }
    strcpy(ctx->current_filename, filename);

    ctx->expected_crc = expected_crc;
    ctx->calculated_crc = 0xFFFFFFFFUL;
    ctx->crc_valid = 0;

    logf_debug("Started CRC verification for file %s, expected CRC: 0x%08X", filename, expected_crc);
    return BINKP_OK;
}

ftn_binkp_error_t ftn_crc_update_file(ftn_crc_context_t* ctx, const uint8_t* data, size_t len) {
    if (!ctx || !data) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    if (!ctx->crc_negotiated) {
        return BINKP_OK;
    }

    ctx->calculated_crc = ftn_crc32_update(ctx->calculated_crc, data, len);
    return BINKP_OK;
}

ftn_binkp_error_t ftn_crc_finish_file(ftn_crc_context_t* ctx, int* crc_valid) {
    uint32_t final_crc;

    if (!ctx || !crc_valid) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    *crc_valid = 0;

    if (!ctx->crc_negotiated) {
        *crc_valid = 1; /* No CRC checking - assume valid */
        return BINKP_OK;
    }

    final_crc = ctx->calculated_crc ^ 0xFFFFFFFFUL;
    ctx->crc_valid = (final_crc == ctx->expected_crc);
    *crc_valid = ctx->crc_valid;

    /* Update statistics */
    if (ctx->crc_valid) {
        ctx->files_verified++;
        logf_info("CRC verification passed for %s: 0x%08X", ctx->current_filename, final_crc);
    } else {
        ctx->files_failed++;
        logf_warning("CRC verification failed for %s: expected 0x%08X, got 0x%08X",
                       ctx->current_filename, ctx->expected_crc, final_crc);
    }

    return BINKP_OK;
}

ftn_binkp_error_t ftn_crc_create_command(const ftn_crc_context_t* ctx, const char* filename,
                                         uint32_t size, uint32_t crc, char** command) {
    if (!ctx || !filename || !command) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    if (!ctx->crc_negotiated) {
        *command = NULL;
        return BINKP_OK;
    }

    /* Create CRC command string */
    *command = malloc(256);
    if (!*command) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    snprintf(*command, 256, "CRC %s %u 0x%08X", filename, size, crc);
    logf_debug("Created CRC command: %s", *command);
    return BINKP_OK;
}

ftn_binkp_error_t ftn_crc_parse_command(const char* command, ftn_crc_file_info_t* file_info) {
    char* cmd_copy;
    char* token;
    char* saveptr;
    int token_count = 0;

    if (!command || !file_info) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    memset(file_info, 0, sizeof(ftn_crc_file_info_t));

    /* Check if this is a CRC command */
    if (strncmp(command, "CRC ", 4) != 0) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    cmd_copy = malloc(strlen(command) + 1);
    if (!cmd_copy) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }
    strcpy(cmd_copy, command);

    /* Parse CRC filename size crc */
    token = strtok_r(cmd_copy, " ", &saveptr);
    while (token && token_count < 4) {
        switch (token_count) {
            case 0: /* "CRC" */
                if (strcmp(token, "CRC") != 0) {
                    free(cmd_copy);
                    return BINKP_ERROR_INVALID_COMMAND;
                }
                break;
            case 1: /* filename */
                file_info->filename = malloc(strlen(token) + 1);
                if (file_info->filename) {
                    strcpy(file_info->filename, token);
                }
                break;
            case 2: /* size */
                file_info->size = (uint32_t)strtoul(token, NULL, 10);
                break;
            case 3: /* crc */
                file_info->crc32 = (uint32_t)strtoul(token, NULL, 0);
                break;
        }
        token_count++;
        token = strtok_r(NULL, " ", &saveptr);
    }

    free(cmd_copy);

    if (token_count != 4) {
        if (file_info->filename) {
            free(file_info->filename);
            file_info->filename = NULL;
        }
        return BINKP_ERROR_INVALID_COMMAND;
    }

    return BINKP_OK;
}

ftn_binkp_error_t ftn_crc_parse_option(const char* option, ftn_crc_mode_t* mode, ftn_crc_algorithm_t* algorithm) {
    if (!option || !mode || !algorithm) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    *algorithm = CRC_ALGORITHM_NONE;

    if (strcmp(option, "CRC") == 0) {
        *mode = CRC_MODE_SUPPORTED;
        *algorithm = CRC_ALGORITHM_CRC32;
        return BINKP_OK;
    }

    *mode = CRC_MODE_NONE;
    return BINKP_ERROR_INVALID_COMMAND;
}

ftn_binkp_error_t ftn_crc_verify_frame(ftn_crc_context_t* ctx, const ftn_binkp_frame_t* frame) {
    if (!ctx || !frame) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    if (!ctx->crc_negotiated || frame->is_command) {
        return BINKP_OK;
    }

    /* Update CRC for data frames */
    return ftn_crc_update_file(ctx, frame->data, frame->size);
}

ftn_binkp_error_t ftn_crc_add_frame_crc(ftn_crc_context_t* ctx, ftn_binkp_frame_t* frame) {
    /* This would add CRC information to frames if required by protocol */
    if (!ctx || !frame) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* For this implementation, we don't modify frames */
    return BINKP_OK;
}

int ftn_crc_is_enabled(const ftn_crc_context_t* ctx) {
    return ctx ? ctx->crc_enabled : 0;
}

int ftn_crc_is_negotiated(const ftn_crc_context_t* ctx) {
    return ctx ? ctx->crc_negotiated : 0;
}

const char* ftn_crc_mode_name(ftn_crc_mode_t mode) {
    switch (mode) {
        case CRC_MODE_NONE:
            return "NONE";
        case CRC_MODE_SUPPORTED:
            return "SUPPORTED";
        case CRC_MODE_REQUIRED:
            return "REQUIRED";
        default:
            return "UNKNOWN";
    }
}

ftn_crc_mode_t ftn_crc_mode_from_name(const char* name) {
    if (!name) {
        return CRC_MODE_NONE;
    }

    if (strcasecmp(name, "SUPPORTED") == 0) {
        return CRC_MODE_SUPPORTED;
    } else if (strcasecmp(name, "REQUIRED") == 0) {
        return CRC_MODE_REQUIRED;
    } else if (strcasecmp(name, "NONE") == 0) {
        return CRC_MODE_NONE;
    }

    return CRC_MODE_NONE;
}

const char* ftn_crc_algorithm_name(ftn_crc_algorithm_t algorithm) {
    switch (algorithm) {
        case CRC_ALGORITHM_CRC32:
            return "CRC32";
        case CRC_ALGORITHM_NONE:
        default:
            return "NONE";
    }
}

ftn_crc_algorithm_t ftn_crc_algorithm_from_name(const char* name) {
    if (!name) {
        return CRC_ALGORITHM_NONE;
    }

    if (strcasecmp(name, "CRC32") == 0) {
        return CRC_ALGORITHM_CRC32;
    }

    return CRC_ALGORITHM_NONE;
}

void ftn_crc_get_stats(const ftn_crc_context_t* ctx, uint32_t* files_verified, uint32_t* files_failed, uint32_t* bytes_verified) {
    if (!ctx) {
        return;
    }

    if (files_verified) {
        *files_verified = ctx->files_verified;
    }
    if (files_failed) {
        *files_failed = ctx->files_failed;
    }
    if (bytes_verified) {
        *bytes_verified = ctx->bytes_verified;
    }
}

double ftn_crc_get_success_rate(const ftn_crc_context_t* ctx) {
    uint32_t total_files;

    if (!ctx) {
        return 0.0;
    }

    total_files = ctx->files_verified + ctx->files_failed;
    if (total_files == 0) {
        return 1.0;
    }

    return (double)ctx->files_verified / (double)total_files;
}