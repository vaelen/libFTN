/*
 * binkp_auth.c - Binkp authentication implementation
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "ftn/binkp/auth.h"
#include "ftn/log.h"

ftn_binkp_error_t ftn_binkp_auth_init(ftn_binkp_auth_context_t* auth_ctx, ftn_config_t* config) {
    if (!auth_ctx || !config) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    memset(auth_ctx, 0, sizeof(ftn_binkp_auth_context_t));
    auth_ctx->config = config;
    return BINKP_OK;
}

void ftn_binkp_auth_free(ftn_binkp_auth_context_t* auth_ctx) {
    if (!auth_ctx) {
        return;
    }

    if (auth_ctx->remote_address) {
        free(auth_ctx->remote_address);
        auth_ctx->remote_address = NULL;
    }

    if (auth_ctx->provided_password) {
        free(auth_ctx->provided_password);
        auth_ctx->provided_password = NULL;
    }

    memset(auth_ctx, 0, sizeof(ftn_binkp_auth_context_t));
}

ftn_binkp_auth_result_t ftn_binkp_validate_address(ftn_binkp_auth_context_t* auth_ctx, const char* address_list) {
    char** addresses;
    size_t count;
    size_t i;
    ftn_binkp_error_t result;
    ftn_binkp_auth_result_t auth_result;

    if (!auth_ctx || !address_list) {
        return BINKP_AUTH_INVALID_ADDRESS;
    }

    /* Parse the address list */
    result = ftn_binkp_parse_address_list(address_list, &addresses, &count);
    if (result != BINKP_OK) {
        return BINKP_AUTH_INVALID_ADDRESS;
    }

    auth_result = BINKP_AUTH_INVALID_ADDRESS;

    /* Check each address against our configuration */
    for (i = 0; i < count; i++) {
        size_t j;
        for (j = 0; j < auth_ctx->config->network_count; j++) {
            if (auth_ctx->config->networks[j].address_str &&
                ftn_binkp_address_matches(addresses[i], auth_ctx->config->networks[j].address_str)) {

                /* Found a match, store the remote address */
                if (auth_ctx->remote_address) {
                    free(auth_ctx->remote_address);
                }
                auth_ctx->remote_address = malloc(strlen(addresses[i]) + 1);
                if (auth_ctx->remote_address) {
                    strcpy(auth_ctx->remote_address, addresses[i]);
                }

                logf_info("Validated remote address: %s", addresses[i]);
                auth_result = BINKP_AUTH_SUCCESS;
                break;
            }
        }
        if (auth_result == BINKP_AUTH_SUCCESS) {
            break;
        }
    }

    ftn_binkp_free_address_list(addresses, count);

    if (auth_result != BINKP_AUTH_SUCCESS) {
        logf_warning("No matching address found in: %s", address_list);
    }

    return auth_result;
}

ftn_binkp_error_t ftn_binkp_parse_address_list(const char* address_list, char*** addresses, size_t* count) {
    char* list_copy;
    char* token;
    char* saveptr;
    char** addr_array;
    size_t capacity;
    size_t addr_count;

    if (!address_list || !addresses || !count) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    list_copy = malloc(strlen(address_list) + 1);
    if (!list_copy) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }
    strcpy(list_copy, address_list);

    capacity = 10;
    addr_array = malloc(capacity * sizeof(char*));
    if (!addr_array) {
        free(list_copy);
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    addr_count = 0;
    token = strtok_r(list_copy, " \t", &saveptr);
    while (token && addr_count < capacity) {
        char* normalized;
        if (ftn_binkp_normalize_address(token, &normalized) == BINKP_OK) {
            addr_array[addr_count] = normalized;
            addr_count++;
        }
        token = strtok_r(NULL, " \t", &saveptr);
    }

    free(list_copy);

    *addresses = addr_array;
    *count = addr_count;
    return BINKP_OK;
}

void ftn_binkp_free_address_list(char** addresses, size_t count) {
    size_t i;

    if (!addresses) {
        return;
    }

    for (i = 0; i < count; i++) {
        if (addresses[i]) {
            free(addresses[i]);
        }
    }
    free(addresses);
}

ftn_binkp_auth_result_t ftn_binkp_authenticate_password(ftn_binkp_auth_context_t* auth_ctx, const char* password) {
    char* expected_password;
    ftn_binkp_error_t result;

    if (!auth_ctx || !password) {
        return BINKP_AUTH_FAILED;
    }

    /* Store the provided password */
    if (auth_ctx->provided_password) {
        free(auth_ctx->provided_password);
    }
    auth_ctx->provided_password = malloc(strlen(password) + 1);
    if (auth_ctx->provided_password) {
        strcpy(auth_ctx->provided_password, password);
    }

    /* Look up the expected password for the remote address */
    result = ftn_binkp_lookup_password(auth_ctx->config, auth_ctx->remote_address, &expected_password);
    if (result != BINKP_OK || !expected_password) {
        logf_debug("No password configured for address: %s", auth_ctx->remote_address ? auth_ctx->remote_address : "unknown");
        return BINKP_AUTH_NO_PASSWORD;
    }

    /* Compare passwords */
    if (strcmp(password, expected_password) == 0) {
        auth_ctx->authenticated = 1;
        auth_ctx->is_secure = 1;
        logf_info("Password authentication successful for %s", auth_ctx->remote_address);
        free(expected_password);
        return BINKP_AUTH_SUCCESS;
    } else {
        logf_warning("Password authentication failed for %s", auth_ctx->remote_address);
        free(expected_password);
        return BINKP_AUTH_FAILED;
    }
}

ftn_binkp_error_t ftn_binkp_lookup_password(ftn_config_t* config, const char* address, char** password) {
    size_t i;

    if (!config || !address || !password) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    *password = NULL;

    /* Search through network configurations */
    for (i = 0; i < config->network_count; i++) {
        if (config->networks[i].address_str && config->networks[i].password &&
            ftn_binkp_address_matches(address, config->networks[i].address_str)) {

            *password = malloc(strlen(config->networks[i].password) + 1);
            if (*password) {
                strcpy(*password, config->networks[i].password);
                return BINKP_OK;
            } else {
                return BINKP_ERROR_BUFFER_TOO_SMALL;
            }
        }
    }

    return BINKP_ERROR_INVALID_COMMAND;
}

int ftn_binkp_is_session_secure(const ftn_binkp_auth_context_t* auth_ctx) {
    if (!auth_ctx) {
        return 0;
    }
    return auth_ctx->is_secure;
}

int ftn_binkp_requires_password(ftn_config_t* config, const char* address) {
    char* password;
    ftn_binkp_error_t result;

    if (!config || !address) {
        return 0;
    }

    result = ftn_binkp_lookup_password(config, address, &password);
    if (result == BINKP_OK && password) {
        free(password);
        return 1;
    }

    return 0;
}

const char* ftn_binkp_auth_result_string(ftn_binkp_auth_result_t result) {
    switch (result) {
        case BINKP_AUTH_SUCCESS:
            return "Success";
        case BINKP_AUTH_FAILED:
            return "Authentication failed";
        case BINKP_AUTH_NO_PASSWORD:
            return "No password configured";
        case BINKP_AUTH_INVALID_ADDRESS:
            return "Invalid address";
        default:
            return "Unknown result";
    }
}

ftn_binkp_error_t ftn_binkp_normalize_address(const char* address, char** normalized) {
    size_t len;
    size_t i;
    char* result;

    if (!address || !normalized) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    len = strlen(address);
    result = malloc(len + 1);
    if (!result) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    /* Copy and normalize (basic implementation - just copy for now) */
    for (i = 0; i <= len; i++) {
        result[i] = address[i];
    }

    *normalized = result;
    return BINKP_OK;
}

int ftn_binkp_address_matches(const char* addr1, const char* addr2) {
    if (!addr1 || !addr2) {
        return 0;
    }

    /* Simple string comparison for now */
    return strcmp(addr1, addr2) == 0;
}