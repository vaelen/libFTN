#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ftn/binkp_plz.h"
#include "ftn/log.h"

/* Default buffer sizes */
#define PLZ_DEFAULT_BUFFER_SIZE 8192
#define PLZ_MAX_FRAME_SIZE 32767

ftn_binkp_error_t ftn_plz_init(ftn_plz_context_t* ctx) {
    if (!ctx) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    memset(ctx, 0, sizeof(ftn_plz_context_t));
    ctx->local_mode = PLZ_MODE_NONE;
    ctx->remote_mode = PLZ_MODE_NONE;
    ctx->compression_level = PLZ_LEVEL_DEFAULT;

    /* Initialize buffers */
    ctx->compress_buffer_size = PLZ_DEFAULT_BUFFER_SIZE;
    ctx->compress_buffer = malloc(ctx->compress_buffer_size);
    if (!ctx->compress_buffer) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    ctx->decompress_buffer_size = PLZ_DEFAULT_BUFFER_SIZE;
    ctx->decompress_buffer = malloc(ctx->decompress_buffer_size);
    if (!ctx->decompress_buffer) {
        free(ctx->compress_buffer);
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    return BINKP_OK;
}

void ftn_plz_free(ftn_plz_context_t* ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->compress_buffer) {
        free(ctx->compress_buffer);
        ctx->compress_buffer = NULL;
    }

    if (ctx->decompress_buffer) {
        free(ctx->decompress_buffer);
        ctx->decompress_buffer = NULL;
    }

    /* Note: In real implementation, would free zlib contexts here */
    ctx->compress_ctx = NULL;
    ctx->decompress_ctx = NULL;

    memset(ctx, 0, sizeof(ftn_plz_context_t));
}

ftn_binkp_error_t ftn_plz_set_mode(ftn_plz_context_t* ctx, ftn_plz_mode_t mode) {
    if (!ctx) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    ctx->local_mode = mode;
    ctx->plz_enabled = (mode != PLZ_MODE_NONE);

    logf_debug("Set PLZ mode to %s", ftn_plz_mode_name(mode));
    return BINKP_OK;
}

ftn_binkp_error_t ftn_plz_set_level(ftn_plz_context_t* ctx, ftn_plz_level_t level) {
    if (!ctx) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    ctx->compression_level = level;
    logf_debug("Set PLZ compression level to %s", ftn_plz_level_name(level));
    return BINKP_OK;
}

ftn_binkp_error_t ftn_plz_negotiate(ftn_plz_context_t* ctx, const char* remote_option) {
    ftn_plz_mode_t remote_mode;
    ftn_plz_level_t remote_level;
    ftn_binkp_error_t result;

    if (!ctx || !remote_option) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* Parse remote PLZ option */
    result = ftn_plz_parse_option(remote_option, &remote_mode, &remote_level);
    if (result != BINKP_OK) {
        return result;
    }

    ctx->remote_mode = remote_mode;

    /* Negotiate PLZ mode */
    if (ctx->local_mode == PLZ_MODE_REQUIRED) {
        if (remote_mode == PLZ_MODE_NONE) {
            logf_error("PLZ mode required but remote does not support it");
            return BINKP_ERROR_AUTH_FAILED;
        }
        ctx->plz_negotiated = 1;
    } else if (ctx->local_mode == PLZ_MODE_SUPPORTED) {
        if (remote_mode != PLZ_MODE_NONE) {
            ctx->plz_negotiated = 1;
        }
    } else {
        /* Local mode is NONE */
        if (remote_mode == PLZ_MODE_REQUIRED) {
            logf_error("Remote requires PLZ mode but local does not support it");
            return BINKP_ERROR_AUTH_FAILED;
        }
        ctx->plz_negotiated = 0;
    }

    logf_info("PLZ mode negotiation: local=%s, remote=%s, negotiated=%s",
                 ftn_plz_mode_name(ctx->local_mode),
                 ftn_plz_mode_name(ctx->remote_mode),
                 ctx->plz_negotiated ? "yes" : "no");

    return BINKP_OK;
}

ftn_binkp_error_t ftn_plz_create_option(const ftn_plz_context_t* ctx, char** option) {
    if (!ctx || !option) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    if (ctx->local_mode == PLZ_MODE_NONE) {
        *option = NULL;
        return BINKP_OK;
    }

    /* Create PLZ option string */
    *option = malloc(16);
    if (!*option) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    snprintf(*option, 16, "PLZ");
    return BINKP_OK;
}

ftn_binkp_error_t ftn_plz_compress_data(ftn_plz_context_t* ctx, const uint8_t* input, size_t input_len,
                                        uint8_t** output, size_t* output_len) {
    /* Simplified mock compression - in real implementation would use zlib */
    size_t i;

    if (!ctx || !input || !output || !output_len) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    if (!ctx->plz_negotiated) {
        /* No compression - just copy data */
        *output = malloc(input_len);
        if (!*output) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(*output, input, input_len);
        *output_len = input_len;
        return BINKP_OK;
    }

    /* Mock compression: simple run-length encoding for demonstration */
    *output = malloc(input_len + input_len / 2 + 16); /* Worst case expansion */
    if (!*output) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    *output_len = 0;
    for (i = 0; i < input_len; i++) {
        size_t run_length = 1;

        /* Count consecutive identical bytes */
        while (i + run_length < input_len && input[i] == input[i + run_length] && run_length < 255) {
            run_length++;
        }

        if (run_length >= 3) {
            /* Compress run */
            (*output)[(*output_len)++] = 0xFF; /* Escape byte */
            (*output)[(*output_len)++] = (uint8_t)run_length;
            (*output)[(*output_len)++] = input[i];
            i += run_length - 1;
        } else {
            /* Single byte */
            if (input[i] == 0xFF) {
                /* Escape literal 0xFF */
                (*output)[(*output_len)++] = 0xFF;
                (*output)[(*output_len)++] = 0x01;
                (*output)[(*output_len)++] = 0xFF;
            } else {
                (*output)[(*output_len)++] = input[i];
            }
        }
    }

    /* Update statistics */
    ctx->bytes_sent_uncompressed += input_len;
    ctx->bytes_sent_compressed += *output_len;

    logf_debug("PLZ compressed %zu bytes to %zu bytes (ratio: %.2f%%)",
                  input_len, *output_len,
                  input_len > 0 ? (100.0 * *output_len / input_len) : 0.0);

    return BINKP_OK;
}

ftn_binkp_error_t ftn_plz_decompress_data(ftn_plz_context_t* ctx, const uint8_t* input, size_t input_len,
                                          uint8_t** output, size_t* output_len) {
    /* Simplified mock decompression */
    size_t i, out_pos;

    if (!ctx || !input || !output || !output_len) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    if (!ctx->plz_negotiated) {
        /* No compression - just copy data */
        *output = malloc(input_len);
        if (!*output) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(*output, input, input_len);
        *output_len = input_len;
        return BINKP_OK;
    }

    /* Allocate output buffer (estimate 2x expansion for safety) */
    *output = malloc(input_len * 2 + 256);
    if (!*output) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    /* Mock decompression: reverse of run-length encoding */
    out_pos = 0;
    for (i = 0; i < input_len; i++) {
        if (input[i] == 0xFF && i + 2 < input_len) {
            uint8_t run_length = input[i + 1];
            uint8_t value = input[i + 2];

            if (run_length == 1) {
                /* Escaped literal 0xFF */
                (*output)[out_pos++] = 0xFF;
            } else {
                /* Expand run */
                size_t j;
                for (j = 0; j < run_length; j++) {
                    (*output)[out_pos++] = value;
                }
            }
            i += 2;
        } else {
            /* Literal byte */
            (*output)[out_pos++] = input[i];
        }
    }

    *output_len = out_pos;

    /* Update statistics */
    ctx->bytes_received_compressed += input_len;
    ctx->bytes_received_uncompressed += *output_len;

    logf_debug("PLZ decompressed %zu bytes to %zu bytes", input_len, *output_len);
    return BINKP_OK;
}

ftn_binkp_error_t ftn_plz_compress_frame(ftn_plz_context_t* ctx, const ftn_binkp_frame_t* input_frame,
                                         ftn_binkp_frame_t* output_frame) {
    uint8_t* compressed_data;
    size_t compressed_len;
    ftn_binkp_error_t result;

    if (!ctx || !input_frame || !output_frame) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* Only compress data frames, not command frames */
    if (input_frame->is_command) {
        /* Command frame - copy as-is */
        *output_frame = *input_frame;
        return BINKP_OK;
    }

    /* Compress data */
    result = ftn_plz_compress_data(ctx, input_frame->data, input_frame->size,
                                   &compressed_data, &compressed_len);
    if (result != BINKP_OK) {
        return result;
    }

    /* Check if compression actually helped */
    if (compressed_len >= input_frame->size) {
        /* Compression didn't help - use original data */
        free(compressed_data);
        *output_frame = *input_frame;
        return BINKP_OK;
    }

    /* Create compressed frame */
    output_frame->header[0] = (uint8_t)((compressed_len >> 8) & 0xFF);
    output_frame->header[1] = (uint8_t)(compressed_len & 0xFF);
    output_frame->data = compressed_data;
    output_frame->size = compressed_len;
    output_frame->is_command = 0;

    return BINKP_OK;
}

ftn_binkp_error_t ftn_plz_decompress_frame(ftn_plz_context_t* ctx, const ftn_binkp_frame_t* input_frame,
                                           ftn_binkp_frame_t* output_frame) {
    uint8_t* decompressed_data;
    size_t decompressed_len;
    ftn_binkp_error_t result;

    if (!ctx || !input_frame || !output_frame) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* Only decompress data frames that are marked as compressed */
    if (input_frame->is_command) {
        /* Command frame - copy as-is */
        *output_frame = *input_frame;
        return BINKP_OK;
    }

    /* Decompress data */
    result = ftn_plz_decompress_data(ctx, input_frame->data, input_frame->size,
                                     &decompressed_data, &decompressed_len);
    if (result != BINKP_OK) {
        return result;
    }

    /* Create decompressed frame */
    output_frame->header[0] = (uint8_t)((decompressed_len >> 8) & 0xFF);
    output_frame->header[1] = (uint8_t)(decompressed_len & 0xFF);
    output_frame->data = decompressed_data;
    output_frame->size = decompressed_len;
    output_frame->is_command = 0;

    return BINKP_OK;
}

ftn_binkp_error_t ftn_plz_parse_option(const char* option, ftn_plz_mode_t* mode, ftn_plz_level_t* level) {
    if (!option || !mode || !level) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    *level = PLZ_LEVEL_DEFAULT;

    if (strcmp(option, "PLZ") == 0) {
        *mode = PLZ_MODE_SUPPORTED;
        return BINKP_OK;
    }

    *mode = PLZ_MODE_NONE;
    return BINKP_ERROR_INVALID_COMMAND;
}

ftn_binkp_error_t ftn_plz_ensure_buffer(ftn_plz_context_t* ctx, size_t min_size) {
    uint8_t* new_buffer;

    if (!ctx) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    if (ctx->compress_buffer_size < min_size) {
        new_buffer = realloc(ctx->compress_buffer, min_size);
        if (!new_buffer) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }
        ctx->compress_buffer = new_buffer;
        ctx->compress_buffer_size = min_size;
    }

    if (ctx->decompress_buffer_size < min_size) {
        new_buffer = realloc(ctx->decompress_buffer, min_size);
        if (!new_buffer) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }
        ctx->decompress_buffer = new_buffer;
        ctx->decompress_buffer_size = min_size;
    }

    return BINKP_OK;
}

int ftn_plz_is_enabled(const ftn_plz_context_t* ctx) {
    return ctx ? ctx->plz_enabled : 0;
}

int ftn_plz_is_negotiated(const ftn_plz_context_t* ctx) {
    return ctx ? ctx->plz_negotiated : 0;
}

const char* ftn_plz_mode_name(ftn_plz_mode_t mode) {
    switch (mode) {
        case PLZ_MODE_NONE:
            return "NONE";
        case PLZ_MODE_SUPPORTED:
            return "SUPPORTED";
        case PLZ_MODE_REQUIRED:
            return "REQUIRED";
        default:
            return "UNKNOWN";
    }
}

ftn_plz_mode_t ftn_plz_mode_from_name(const char* name) {
    if (!name) {
        return PLZ_MODE_NONE;
    }

    if (strcasecmp(name, "SUPPORTED") == 0) {
        return PLZ_MODE_SUPPORTED;
    } else if (strcasecmp(name, "REQUIRED") == 0) {
        return PLZ_MODE_REQUIRED;
    } else if (strcasecmp(name, "NONE") == 0) {
        return PLZ_MODE_NONE;
    }

    return PLZ_MODE_NONE;
}

const char* ftn_plz_level_name(ftn_plz_level_t level) {
    switch (level) {
        case PLZ_LEVEL_DEFAULT:
            return "DEFAULT";
        case PLZ_LEVEL_FAST:
            return "FAST";
        case PLZ_LEVEL_NORMAL:
            return "NORMAL";
        case PLZ_LEVEL_BEST:
            return "BEST";
        default:
            return "UNKNOWN";
    }
}

ftn_plz_level_t ftn_plz_level_from_name(const char* name) {
    if (!name) {
        return PLZ_LEVEL_DEFAULT;
    }

    if (strcasecmp(name, "FAST") == 0) {
        return PLZ_LEVEL_FAST;
    } else if (strcasecmp(name, "NORMAL") == 0) {
        return PLZ_LEVEL_NORMAL;
    } else if (strcasecmp(name, "BEST") == 0) {
        return PLZ_LEVEL_BEST;
    } else if (strcasecmp(name, "DEFAULT") == 0) {
        return PLZ_LEVEL_DEFAULT;
    }

    return PLZ_LEVEL_DEFAULT;
}

void ftn_plz_get_stats(const ftn_plz_context_t* ctx, uint32_t* sent_uncompressed, uint32_t* sent_compressed,
                       uint32_t* received_compressed, uint32_t* received_uncompressed) {
    if (!ctx) {
        return;
    }

    if (sent_uncompressed) {
        *sent_uncompressed = ctx->bytes_sent_uncompressed;
    }
    if (sent_compressed) {
        *sent_compressed = ctx->bytes_sent_compressed;
    }
    if (received_compressed) {
        *received_compressed = ctx->bytes_received_compressed;
    }
    if (received_uncompressed) {
        *received_uncompressed = ctx->bytes_received_uncompressed;
    }
}

double ftn_plz_get_compression_ratio(const ftn_plz_context_t* ctx) {
    if (!ctx || ctx->bytes_sent_uncompressed == 0) {
        return 1.0;
    }

    return (double)ctx->bytes_sent_compressed / (double)ctx->bytes_sent_uncompressed;
}