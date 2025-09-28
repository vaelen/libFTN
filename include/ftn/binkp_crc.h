/*
 * binkp_crc.h - CRC Checksum Verification for binkp protocol (FTS-1030)
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

#ifndef FTN_BINKP_CRC_H
#define FTN_BINKP_CRC_H

#include <stdint.h>
#include <stddef.h>
#include "binkp.h"

/* CRC verification modes */
typedef enum {
    CRC_MODE_NONE = 0,
    CRC_MODE_SUPPORTED = 1,
    CRC_MODE_REQUIRED = 2
} ftn_crc_mode_t;

/* CRC algorithm types */
typedef enum {
    CRC_ALGORITHM_NONE = 0,
    CRC_ALGORITHM_CRC32 = 1
} ftn_crc_algorithm_t;

/* CRC context structure */
typedef struct {
    int crc_enabled;
    int crc_negotiated;
    ftn_crc_mode_t local_mode;
    ftn_crc_mode_t remote_mode;
    ftn_crc_algorithm_t algorithm;

    /* Current file CRC calculation */
    char* current_filename;
    uint32_t calculated_crc;
    uint32_t expected_crc;
    int crc_valid;

    /* Statistics */
    uint32_t files_verified;
    uint32_t files_failed;
    uint32_t bytes_verified;
} ftn_crc_context_t;

/* File CRC information */
typedef struct {
    char* filename;
    uint32_t size;
    uint32_t crc32;
    uint32_t timestamp;
} ftn_crc_file_info_t;

/* CRC operations */
ftn_binkp_error_t ftn_crc_init(ftn_crc_context_t* ctx);
void ftn_crc_free(ftn_crc_context_t* ctx);

/* CRC mode negotiation */
ftn_binkp_error_t ftn_crc_set_mode(ftn_crc_context_t* ctx, ftn_crc_mode_t mode);
ftn_binkp_error_t ftn_crc_negotiate(ftn_crc_context_t* ctx, const char* remote_option);
ftn_binkp_error_t ftn_crc_create_option(const ftn_crc_context_t* ctx, char** option);

/* CRC calculation */
uint32_t ftn_crc32_calculate(const uint8_t* data, size_t len);
uint32_t ftn_crc32_update(uint32_t crc, const uint8_t* data, size_t len);
ftn_binkp_error_t ftn_crc32_file(const char* filename, uint32_t* crc);

/* File CRC operations */
ftn_binkp_error_t ftn_crc_start_file(ftn_crc_context_t* ctx, const char* filename, uint32_t expected_crc);
ftn_binkp_error_t ftn_crc_update_file(ftn_crc_context_t* ctx, const uint8_t* data, size_t len);
ftn_binkp_error_t ftn_crc_finish_file(ftn_crc_context_t* ctx, int* crc_valid);

/* CRC command generation and parsing */
ftn_binkp_error_t ftn_crc_create_command(const ftn_crc_context_t* ctx, const char* filename,
                                         uint32_t size, uint32_t crc, char** command);
ftn_binkp_error_t ftn_crc_parse_command(const char* command, ftn_crc_file_info_t* file_info);

/* CRC option parsing */
ftn_binkp_error_t ftn_crc_parse_option(const char* option, ftn_crc_mode_t* mode, ftn_crc_algorithm_t* algorithm);

/* Frame CRC verification */
ftn_binkp_error_t ftn_crc_verify_frame(ftn_crc_context_t* ctx, const ftn_binkp_frame_t* frame);
ftn_binkp_error_t ftn_crc_add_frame_crc(ftn_crc_context_t* ctx, ftn_binkp_frame_t* frame);

/* Utility functions */
int ftn_crc_is_enabled(const ftn_crc_context_t* ctx);
int ftn_crc_is_negotiated(const ftn_crc_context_t* ctx);
const char* ftn_crc_mode_name(ftn_crc_mode_t mode);
ftn_crc_mode_t ftn_crc_mode_from_name(const char* name);
const char* ftn_crc_algorithm_name(ftn_crc_algorithm_t algorithm);
ftn_crc_algorithm_t ftn_crc_algorithm_from_name(const char* name);

/* Statistics */
void ftn_crc_get_stats(const ftn_crc_context_t* ctx, uint32_t* files_verified, uint32_t* files_failed, uint32_t* bytes_verified);
double ftn_crc_get_success_rate(const ftn_crc_context_t* ctx);

/* CRC table initialization (call once at startup) */
void ftn_crc32_init_table(void);

#endif /* FTN_BINKP_CRC_H */