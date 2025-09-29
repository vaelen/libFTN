/*
 * binkp_plz.h - Dataframe Compression for binkp protocol (FTS-1029)
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

#ifndef FTN_BINKP_PLZ_H
#define FTN_BINKP_PLZ_H

#include <stdint.h>
#include <stddef.h>
#include "../binkp.h"
#include "zlib.h"

/* PLZ compression modes */
typedef enum {
    PLZ_MODE_NONE = 0,
    PLZ_MODE_SUPPORTED = 1,
    PLZ_MODE_REQUIRED = 2
} ftn_plz_mode_t;

/* PLZ compression levels */
typedef enum {
    PLZ_LEVEL_DEFAULT = 0,
    PLZ_LEVEL_FAST = 1,
    PLZ_LEVEL_NORMAL = 6,
    PLZ_LEVEL_BEST = 9
} ftn_plz_level_t;

/* PLZ context structure */
typedef struct {
    int plz_enabled;
    int plz_negotiated;
    ftn_plz_mode_t local_mode;
    ftn_plz_mode_t remote_mode;
    ftn_plz_level_t compression_level;

    /* Real zlib compression contexts */
    z_stream compress_stream;
    z_stream decompress_stream;
    int compress_initialized;
    int decompress_initialized;

    /* Statistics */
    uint32_t bytes_sent_uncompressed;
    uint32_t bytes_sent_compressed;
    uint32_t bytes_received_compressed;
    uint32_t bytes_received_uncompressed;

    /* Buffer management */
    uint8_t* compress_buffer;
    size_t compress_buffer_size;
    uint8_t* decompress_buffer;
    size_t decompress_buffer_size;
} ftn_plz_context_t;

/* PLZ operations */
ftn_binkp_error_t ftn_plz_init(ftn_plz_context_t* ctx);
void ftn_plz_free(ftn_plz_context_t* ctx);

/* PLZ mode negotiation */
ftn_binkp_error_t ftn_plz_set_mode(ftn_plz_context_t* ctx, ftn_plz_mode_t mode);
ftn_binkp_error_t ftn_plz_set_level(ftn_plz_context_t* ctx, ftn_plz_level_t level);
ftn_binkp_error_t ftn_plz_configure_from_network(ftn_plz_context_t* ctx, const void* network_config);
ftn_binkp_error_t ftn_plz_negotiate(ftn_plz_context_t* ctx, const char* remote_option);
ftn_binkp_error_t ftn_plz_create_option(const ftn_plz_context_t* ctx, char** option);

/* Compression operations */
ftn_binkp_error_t ftn_plz_compress_data(ftn_plz_context_t* ctx, const uint8_t* input, size_t input_len,
                                        uint8_t** output, size_t* output_len);
ftn_binkp_error_t ftn_plz_decompress_data(ftn_plz_context_t* ctx, const uint8_t* input, size_t input_len,
                                          uint8_t** output, size_t* output_len);

/* Frame compression for binkp */
ftn_binkp_error_t ftn_plz_compress_frame(ftn_plz_context_t* ctx, const ftn_binkp_frame_t* input_frame,
                                         ftn_binkp_frame_t* output_frame);
ftn_binkp_error_t ftn_plz_decompress_frame(ftn_plz_context_t* ctx, const ftn_binkp_frame_t* input_frame,
                                           ftn_binkp_frame_t* output_frame);

/* PLZ option parsing */
ftn_binkp_error_t ftn_plz_parse_option(const char* option, ftn_plz_mode_t* mode, ftn_plz_level_t* level);

/* Buffer management */
ftn_binkp_error_t ftn_plz_ensure_buffer(ftn_plz_context_t* ctx, size_t min_size);

/* Utility functions */
int ftn_plz_is_enabled(const ftn_plz_context_t* ctx);
int ftn_plz_is_negotiated(const ftn_plz_context_t* ctx);
const char* ftn_plz_mode_name(ftn_plz_mode_t mode);
ftn_plz_mode_t ftn_plz_mode_from_name(const char* name);
const char* ftn_plz_level_name(ftn_plz_level_t level);
ftn_plz_level_t ftn_plz_level_from_name(const char* name);

/* Statistics */
void ftn_plz_get_stats(const ftn_plz_context_t* ctx, uint32_t* sent_uncompressed, uint32_t* sent_compressed,
                       uint32_t* received_compressed, uint32_t* received_uncompressed);
double ftn_plz_get_compression_ratio(const ftn_plz_context_t* ctx);

#endif /* FTN_BINKP_PLZ_H */