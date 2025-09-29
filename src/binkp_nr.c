#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "ftn/binkp_nr.h"
#include "ftn/log.h"

ftn_binkp_error_t ftn_nr_init(ftn_nr_context_t* ctx) {
    if (!ctx) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    memset(ctx, 0, sizeof(ftn_nr_context_t));
    ctx->local_mode = NR_MODE_NONE;
    ctx->remote_mode = NR_MODE_NONE;

    return BINKP_OK;
}

void ftn_nr_free(ftn_nr_context_t* ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->current_filename) {
        free(ctx->current_filename);
        ctx->current_filename = NULL;
    }

    if (ctx->nr_option) {
        free(ctx->nr_option);
        ctx->nr_option = NULL;
    }

    if (ctx->nda_option) {
        free(ctx->nda_option);
        ctx->nda_option = NULL;
    }

    memset(ctx, 0, sizeof(ftn_nr_context_t));
}

ftn_binkp_error_t ftn_nr_set_mode(ftn_nr_context_t* ctx, ftn_nr_mode_t mode) {
    if (!ctx) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    ctx->local_mode = mode;
    ctx->nr_enabled = (mode != NR_MODE_NONE);

    logf_debug("Set NR mode to %s", ftn_nr_mode_name(mode));
    return BINKP_OK;
}

ftn_binkp_error_t ftn_nr_negotiate(ftn_nr_context_t* ctx, const char* remote_option) {
    ftn_nr_mode_t remote_mode;
    ftn_binkp_error_t result;

    if (!ctx || !remote_option) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* Parse remote NR option */
    result = ftn_nr_parse_option(remote_option, &remote_mode);
    if (result != BINKP_OK) {
        return result;
    }

    ctx->remote_mode = remote_mode;

    /* Negotiate NR mode */
    if (ctx->local_mode == NR_MODE_REQUIRED) {
        if (remote_mode == NR_MODE_NONE) {
            logf_error("NR mode required but remote does not support it");
            return BINKP_ERROR_AUTH_FAILED;
        }
        ctx->nr_negotiated = 1;
    } else if (ctx->local_mode == NR_MODE_SUPPORTED) {
        if (remote_mode != NR_MODE_NONE) {
            ctx->nr_negotiated = 1;
        }
    } else {
        /* Local mode is NONE */
        if (remote_mode == NR_MODE_REQUIRED) {
            logf_error("Remote requires NR mode but local does not support it");
            return BINKP_ERROR_AUTH_FAILED;
        }
        ctx->nr_negotiated = 0;
    }

    logf_info("NR mode negotiation: local=%s, remote=%s, negotiated=%s",
                 ftn_nr_mode_name(ctx->local_mode),
                 ftn_nr_mode_name(ctx->remote_mode),
                 ctx->nr_negotiated ? "yes" : "no");

    return BINKP_OK;
}

ftn_binkp_error_t ftn_nr_create_option(const ftn_nr_context_t* ctx, char** option) {
    if (!ctx || !option) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    if (ctx->local_mode == NR_MODE_NONE) {
        *option = NULL;
        return BINKP_OK;
    }

    /* Create NR option string */
    *option = malloc(16);
    if (!*option) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    snprintf(*option, 16, "NR");
    return BINKP_OK;
}

ftn_binkp_error_t ftn_nr_set_file_info(ftn_nr_context_t* ctx, const char* filename,
                                       uint32_t size, uint32_t timestamp, uint32_t offset) {
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

    ctx->expected_size = size;
    ctx->resume_offset = offset;

    logf_debug("Set NR file info: %s, size=%u, timestamp=%u, offset=%u",
                  filename, size, timestamp, offset);
    return BINKP_OK;
}

ftn_binkp_error_t ftn_nr_get_resume_offset(const ftn_nr_context_t* ctx, const char* filename, uint32_t* offset) {
    uint32_t existing_size;
    ftn_binkp_error_t result;

    if (!ctx || !filename || !offset) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    *offset = 0;

    if (!ctx->nr_negotiated) {
        return BINKP_OK;
    }

    /* Check if partial file exists */
    result = ftn_nr_check_partial_file(filename, ctx->expected_size, &existing_size);
    if (result == BINKP_OK && existing_size > 0 && existing_size < ctx->expected_size) {
        *offset = existing_size;
        logf_info("Found partial file %s, resume at offset %u", filename, *offset);
    }

    return BINKP_OK;
}

ftn_binkp_error_t ftn_nr_create_nda_response(const ftn_nr_context_t* ctx, const char* filename,
                                             uint32_t size, uint32_t timestamp, char** response) {
    uint32_t offset;
    ftn_binkp_error_t result;

    if (!ctx || !filename || !response) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* Get resume offset */
    result = ftn_nr_get_resume_offset(ctx, filename, &offset);
    if (result != BINKP_OK) {
        return result;
    }

    /* Create NDA response */
    *response = malloc(256);
    if (!*response) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    snprintf(*response, 256, "NDA %s %u %u %u", filename, size, timestamp, offset);
    logf_debug("Created NDA response: %s", *response);
    return BINKP_OK;
}

ftn_binkp_error_t ftn_nr_parse_option(const char* option, ftn_nr_mode_t* mode) {
    if (!option || !mode) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    if (strcmp(option, "NR") == 0) {
        *mode = NR_MODE_SUPPORTED;
        return BINKP_OK;
    }

    *mode = NR_MODE_NONE;
    return BINKP_ERROR_INVALID_COMMAND;
}

ftn_binkp_error_t ftn_nr_parse_nda_option(const char* option, ftn_nr_file_info_t* file_info) {
    char* opt_copy;
    char* token;
    char* saveptr;
    int token_count = 0;

    if (!option || !file_info) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    memset(file_info, 0, sizeof(ftn_nr_file_info_t));

    /* Check if this is an NDA option */
    if (strncmp(option, "NDA ", 4) != 0) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    opt_copy = malloc(strlen(option) + 1);
    if (!opt_copy) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }
    strcpy(opt_copy, option);

    /* Parse NDA filename size timestamp offset */
    token = strtok_r(opt_copy, " ", &saveptr);
    while (token && token_count < 5) {
        switch (token_count) {
            case 0: /* "NDA" */
                if (strcmp(token, "NDA") != 0) {
                    free(opt_copy);
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
            case 3: /* timestamp */
                file_info->timestamp = (uint32_t)strtoul(token, NULL, 10);
                break;
            case 4: /* offset */
                file_info->offset = (uint32_t)strtoul(token, NULL, 10);
                break;
        }
        token_count++;
        token = strtok_r(NULL, " ", &saveptr);
    }

    free(opt_copy);

    if (token_count != 5) {
        if (file_info->filename) {
            free(file_info->filename);
            file_info->filename = NULL;
        }
        return BINKP_ERROR_INVALID_COMMAND;
    }

    return BINKP_OK;
}

ftn_binkp_error_t ftn_nr_parse_nda_response(const char* response, uint32_t* offset) {
    char* resp_copy;
    char* token;
    char* saveptr;
    int token_count = 0;

    if (!response || !offset) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    *offset = 0;

    /* Check if this is an NDA response */
    if (strncmp(response, "NDA ", 4) != 0) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    resp_copy = malloc(strlen(response) + 1);
    if (!resp_copy) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }
    strcpy(resp_copy, response);

    /* Parse NDA filename size timestamp offset */
    token = strtok_r(resp_copy, " ", &saveptr);
    while (token && token_count < 5) {
        if (token_count == 4) { /* offset */
            *offset = (uint32_t)strtoul(token, NULL, 10);
            break;
        }
        token_count++;
        token = strtok_r(NULL, " ", &saveptr);
    }

    free(resp_copy);
    return BINKP_OK;
}

ftn_binkp_error_t ftn_nr_check_partial_file(const char* filename, uint32_t expected_size, uint32_t* existing_size) {
    struct stat st;

    if (!filename || !existing_size) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    *existing_size = 0;

    if (stat(filename, &st) == 0) {
        if (S_ISREG(st.st_mode)) {
            *existing_size = (uint32_t)st.st_size;

            /* Validate file size is reasonable */
            if (*existing_size > expected_size) {
                logf_warning("Partial file %s is larger than expected (%u > %u)",
                               filename, *existing_size, expected_size);
                return BINKP_ERROR_INVALID_COMMAND;
            }

            return BINKP_OK;
        }
    }

    return BINKP_ERROR_PROTOCOL_ERROR;
}

ftn_binkp_error_t ftn_nr_create_partial_file(const char* filename, uint32_t offset) {
    int fd;

    if (!filename) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    if (offset == 0) {
        /* Create new file */
        fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    } else {
        /* Open existing file for append */
        fd = open(filename, O_WRONLY | O_APPEND);
    }

    if (fd < 0) {
        logf_error("Failed to create/open partial file %s", filename);
        return BINKP_ERROR_PROTOCOL_ERROR;
    }

    close(fd);
    logf_debug("Created/opened partial file %s at offset %u", filename, offset);
    return BINKP_OK;
}

int ftn_nr_is_enabled(const ftn_nr_context_t* ctx) {
    return ctx ? ctx->nr_enabled : 0;
}

int ftn_nr_is_negotiated(const ftn_nr_context_t* ctx) {
    return ctx ? ctx->nr_negotiated : 0;
}

const char* ftn_nr_mode_name(ftn_nr_mode_t mode) {
    switch (mode) {
        case NR_MODE_NONE:
            return "NONE";
        case NR_MODE_SUPPORTED:
            return "SUPPORTED";
        case NR_MODE_REQUIRED:
            return "REQUIRED";
        default:
            return "UNKNOWN";
    }
}

ftn_nr_mode_t ftn_nr_mode_from_name(const char* name) {
    if (!name) {
        return NR_MODE_NONE;
    }

    if (strcasecmp(name, "SUPPORTED") == 0) {
        return NR_MODE_SUPPORTED;
    } else if (strcasecmp(name, "REQUIRED") == 0) {
        return NR_MODE_REQUIRED;
    } else if (strcasecmp(name, "NONE") == 0) {
        return NR_MODE_NONE;
    }

    return NR_MODE_NONE;
}