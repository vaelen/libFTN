/*
 * binkp_auth.h - Binkp authentication for libFTN
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

#ifndef FTN_BINKP_AUTH_H
#define FTN_BINKP_AUTH_H

#include <stddef.h>
#include "binkp.h"
#include "config.h"

/* Authentication result */
typedef enum {
    BINKP_AUTH_SUCCESS,
    BINKP_AUTH_FAILED,
    BINKP_AUTH_NO_PASSWORD,
    BINKP_AUTH_INVALID_ADDRESS
} ftn_binkp_auth_result_t;

/* Authentication context */
typedef struct {
    ftn_config_t* config;
    char* remote_address;
    char* provided_password;
    int is_secure;
    int authenticated;
} ftn_binkp_auth_context_t;

/* Authentication operations */
ftn_binkp_error_t ftn_binkp_auth_init(ftn_binkp_auth_context_t* auth_ctx, ftn_config_t* config);
void ftn_binkp_auth_free(ftn_binkp_auth_context_t* auth_ctx);

/* Address validation */
ftn_binkp_auth_result_t ftn_binkp_validate_address(ftn_binkp_auth_context_t* auth_ctx, const char* address_list);
ftn_binkp_error_t ftn_binkp_parse_address_list(const char* address_list, char*** addresses, size_t* count);
void ftn_binkp_free_address_list(char** addresses, size_t count);

/* Password authentication */
ftn_binkp_auth_result_t ftn_binkp_authenticate_password(ftn_binkp_auth_context_t* auth_ctx, const char* password);
ftn_binkp_error_t ftn_binkp_lookup_password(ftn_config_t* config, const char* address, char** password);

/* Security level management */
int ftn_binkp_is_session_secure(const ftn_binkp_auth_context_t* auth_ctx);
int ftn_binkp_requires_password(ftn_config_t* config, const char* address);

/* Utility functions */
const char* ftn_binkp_auth_result_string(ftn_binkp_auth_result_t result);
ftn_binkp_error_t ftn_binkp_normalize_address(const char* address, char** normalized);
int ftn_binkp_address_matches(const char* addr1, const char* addr2);

#endif /* FTN_BINKP_AUTH_H */