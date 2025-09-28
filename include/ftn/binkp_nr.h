/*
 * binkp_nr.h - Non-Reliable Mode for binkp protocol (FTS-1028)
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

#ifndef FTN_BINKP_NR_H
#define FTN_BINKP_NR_H

#include <stdint.h>
#include <stddef.h>
#include "binkp.h"

/* NR Mode capabilities */
typedef enum {
    NR_MODE_NONE = 0,
    NR_MODE_SUPPORTED = 1,
    NR_MODE_REQUIRED = 2
} ftn_nr_mode_t;

/* NR context structure */
typedef struct {
    int nr_enabled;
    int nr_negotiated;
    ftn_nr_mode_t local_mode;
    ftn_nr_mode_t remote_mode;

    /* File resume information */
    char* current_filename;
    uint32_t resume_offset;
    uint32_t expected_size;

    /* NR option strings */
    char* nr_option;
    char* nda_option;
} ftn_nr_context_t;

/* File information for NR mode */
typedef struct {
    char* filename;
    uint32_t size;
    uint32_t timestamp;
    uint32_t offset;
} ftn_nr_file_info_t;

/* NR operations */
ftn_binkp_error_t ftn_nr_init(ftn_nr_context_t* ctx);
void ftn_nr_free(ftn_nr_context_t* ctx);

/* NR mode negotiation */
ftn_binkp_error_t ftn_nr_set_mode(ftn_nr_context_t* ctx, ftn_nr_mode_t mode);
ftn_binkp_error_t ftn_nr_negotiate(ftn_nr_context_t* ctx, const char* remote_option);
ftn_binkp_error_t ftn_nr_create_option(const ftn_nr_context_t* ctx, char** option);

/* File resume operations */
ftn_binkp_error_t ftn_nr_set_file_info(ftn_nr_context_t* ctx, const char* filename,
                                       uint32_t size, uint32_t timestamp, uint32_t offset);
ftn_binkp_error_t ftn_nr_get_resume_offset(const ftn_nr_context_t* ctx, const char* filename, uint32_t* offset);
ftn_binkp_error_t ftn_nr_create_nda_response(const ftn_nr_context_t* ctx, const char* filename,
                                             uint32_t size, uint32_t timestamp, char** response);

/* NR option parsing */
ftn_binkp_error_t ftn_nr_parse_option(const char* option, ftn_nr_mode_t* mode);
ftn_binkp_error_t ftn_nr_parse_nda_option(const char* option, ftn_nr_file_info_t* file_info);
ftn_binkp_error_t ftn_nr_parse_nda_response(const char* response, uint32_t* offset);

/* File operations for resume */
ftn_binkp_error_t ftn_nr_check_partial_file(const char* filename, uint32_t expected_size, uint32_t* existing_size);
ftn_binkp_error_t ftn_nr_create_partial_file(const char* filename, uint32_t offset);

/* Utility functions */
int ftn_nr_is_enabled(const ftn_nr_context_t* ctx);
int ftn_nr_is_negotiated(const ftn_nr_context_t* ctx);
const char* ftn_nr_mode_name(ftn_nr_mode_t mode);
ftn_nr_mode_t ftn_nr_mode_from_name(const char* name);

#endif /* FTN_BINKP_NR_H */