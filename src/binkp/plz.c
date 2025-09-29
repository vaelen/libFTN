/*
 * binkp_plz.c - Compression support for BinkP protocol
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
#include "ftn/binkp/plz.h"
#include "ftn/log.h"
#include "ftn/config.h"
#include "zlib.h"

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

    /* Initialize zlib streams */
    ctx->compress_stream.zalloc = Z_NULL;
    ctx->compress_stream.zfree = Z_NULL;
    ctx->compress_stream.opaque = Z_NULL;
    ctx->compress_initialized = 0;

    ctx->decompress_stream.zalloc = Z_NULL;
    ctx->decompress_stream.zfree = Z_NULL;
    ctx->decompress_stream.opaque = Z_NULL;
    ctx->decompress_initialized = 0;

    return BINKP_OK;
}

void ftn_plz_free(ftn_plz_context_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Clean up zlib streams */
    if (ctx->compress_initialized) {
        deflateEnd(&ctx->compress_stream);
        ctx->compress_initialized = 0;
    }

    if (ctx->decompress_initialized) {
        inflateEnd(&ctx->decompress_stream);
        ctx->decompress_initialized = 0;
    }

    if (ctx->compress_buffer) {
        free(ctx->compress_buffer);
        ctx->compress_buffer = NULL;
    }

    if (ctx->decompress_buffer) {
        free(ctx->decompress_buffer);
        ctx->decompress_buffer = NULL;
    }

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

ftn_binkp_error_t ftn_plz_configure_from_network(ftn_plz_context_t* ctx, const void* network_config) {
    const ftn_network_config_t* net_config;
    ftn_plz_mode_t effective_mode;

    if (!ctx || !network_config) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    net_config = (const ftn_network_config_t*)network_config;

    /* Check if compression is enabled at all */
    if (!net_config->use_compression) {
        /* Compression disabled - set PLZ to none regardless of plz_mode setting */
        effective_mode = PLZ_MODE_NONE;
        logf_debug("Compression disabled via use_compression=no, PLZ set to none");
    } else {
        /* Compression enabled - use the configured PLZ mode */
        effective_mode = (ftn_plz_mode_t)net_config->plz_mode;
        logf_debug("Compression enabled via use_compression=yes, PLZ mode: %s",
                   net_config->plz_mode_str ? net_config->plz_mode_str : "default");
    }

    /* Set the effective PLZ mode */
    ftn_plz_set_mode(ctx, effective_mode);

    /* Set the PLZ compression level */
    ftn_plz_set_level(ctx, (ftn_plz_level_t)net_config->plz_level);

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
    uLongf compressed_len;
    int zlib_level;
    int result;

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

    /* Map PLZ compression level to zlib level */
    switch (ctx->compression_level) {
        case PLZ_LEVEL_FAST:
            zlib_level = Z_BEST_SPEED;
            break;
        case PLZ_LEVEL_BEST:
            zlib_level = Z_BEST_COMPRESSION;
            break;
        case PLZ_LEVEL_NORMAL:
        case PLZ_LEVEL_DEFAULT:
        default:
            zlib_level = Z_DEFAULT_COMPRESSION;
            break;
    }

    /* Calculate maximum compressed size (worst case) */
    compressed_len = compressBound(input_len);
    *output = malloc(compressed_len);
    if (!*output) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    /* Compress using zlib */
    result = compress2(*output, &compressed_len, input, input_len, zlib_level);
    if (result != Z_OK) {
        free(*output);
        *output = NULL;
        logf_error("PLZ compression failed: zlib error %d", result);
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    *output_len = compressed_len;

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
    uLongf decompressed_len;
    int result;
    size_t buffer_size;

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

    /* Estimate decompressed size (start with 4x compression ratio estimate) */
    buffer_size = input_len * 4;
    if (buffer_size < PLZ_DEFAULT_BUFFER_SIZE) {
        buffer_size = PLZ_DEFAULT_BUFFER_SIZE;
    }

    /* Try decompression with increasing buffer sizes if needed */
    do {
        *output = malloc(buffer_size);
        if (!*output) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }

        decompressed_len = buffer_size;
        result = uncompress(*output, &decompressed_len, input, input_len);

        if (result == Z_BUF_ERROR) {
            /* Buffer too small, try larger buffer */
            free(*output);
            buffer_size *= 2;
            if (buffer_size > PLZ_MAX_FRAME_SIZE * 4) {
                /* Prevent runaway buffer growth */
                logf_error("PLZ decompression failed: output too large");
                return BINKP_ERROR_BUFFER_TOO_SMALL;
            }
        } else if (result != Z_OK) {
            free(*output);
            *output = NULL;
            logf_error("PLZ decompression failed: zlib error %d", result);
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }
    } while (result == Z_BUF_ERROR);

    *output_len = decompressed_len;

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