/*
 * binkp_cram.h - CRAM authentication for binkp protocol (FTS-1027)
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

#ifndef FTN_BINKP_CRAM_H
#define FTN_BINKP_CRAM_H

#include <stdint.h>
#include <stddef.h>
#include "../binkp.h"

/* CRAM algorithm types */
typedef enum {
    CRAM_ALGORITHM_NONE,
    CRAM_ALGORITHM_MD5,
    CRAM_ALGORITHM_SHA1
} ftn_cram_algorithm_t;

/* CRAM context structure */
typedef struct {
    char* supported_algorithms[8];
    uint8_t challenge_data[64];
    size_t challenge_len;
    ftn_cram_algorithm_t selected_algorithm;
    char* challenge_hex;
    int challenge_generated;
} ftn_cram_context_t;

/* CRAM operations */
ftn_binkp_error_t ftn_cram_init(ftn_cram_context_t* ctx);
void ftn_cram_free(ftn_cram_context_t* ctx);

/* Challenge generation and parsing */
ftn_binkp_error_t ftn_cram_generate_challenge(ftn_cram_context_t* ctx, ftn_cram_algorithm_t algorithm);
ftn_binkp_error_t ftn_cram_parse_challenge(const char* opt_string, ftn_cram_context_t* ctx);
ftn_binkp_error_t ftn_cram_create_challenge_opt(const ftn_cram_context_t* ctx, char** opt_string);

/* Response generation and verification */
ftn_binkp_error_t ftn_cram_create_response(const char* password, const ftn_cram_context_t* ctx, char** response);
ftn_binkp_error_t ftn_cram_verify_response(const char* password, const ftn_cram_context_t* ctx, const char* response);
ftn_binkp_error_t ftn_cram_parse_response(const char* pwd_string, ftn_cram_algorithm_t* algorithm, char** response);

/* HMAC implementations */
ftn_binkp_error_t ftn_hmac_md5(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len, uint8_t* digest);
ftn_binkp_error_t ftn_hmac_sha1(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len, uint8_t* digest);

/* Hash function implementations */
ftn_binkp_error_t ftn_md5_hash(const uint8_t* data, size_t len, uint8_t* digest);
ftn_binkp_error_t ftn_sha1_hash(const uint8_t* data, size_t len, uint8_t* digest);

/* Utility functions */
char* ftn_bytes_to_hex(const uint8_t* bytes, size_t len, int lowercase);
ftn_binkp_error_t ftn_hex_to_bytes(const char* hex, uint8_t** bytes, size_t* len);
const char* ftn_cram_algorithm_name(ftn_cram_algorithm_t algorithm);
ftn_cram_algorithm_t ftn_cram_algorithm_from_name(const char* name);

/* Random number generation */
ftn_binkp_error_t ftn_generate_random_bytes(uint8_t* buffer, size_t len);

/* CRAM integration with binkp session */
ftn_binkp_error_t ftn_cram_add_supported_algorithms(ftn_cram_context_t* ctx, const char* algorithms);
int ftn_cram_is_supported(const ftn_cram_context_t* ctx, ftn_cram_algorithm_t algorithm);
ftn_binkp_error_t ftn_cram_select_best_algorithm(const ftn_cram_context_t* ctx, ftn_cram_algorithm_t* algorithm);

/* Timing attack protection */
ftn_binkp_error_t ftn_cram_secure_compare(const char* a, const char* b, size_t len);

#endif /* FTN_BINKP_CRAM_H */