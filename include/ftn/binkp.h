/*
 * binkp.h - Binkp protocol implementation for libFTN
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

#ifndef FTN_BINKP_H
#define FTN_BINKP_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdio.h>
#include "compat.h"
#include "net.h"

/* Binkp protocol constants */
#define BINKP_MAX_FRAME_SIZE 32767
#define BINKP_HEADER_SIZE 2
#define BINKP_T_BIT 0x8000

/* Error codes */
typedef enum {
    BINKP_OK = 0,
    BINKP_ERROR_INVALID_FRAME,
    BINKP_ERROR_FRAME_TOO_LARGE,
    BINKP_ERROR_BUFFER_TOO_SMALL,
    BINKP_ERROR_INVALID_COMMAND,
    BINKP_ERROR_NETWORK,
    BINKP_ERROR_TIMEOUT,
    BINKP_ERROR_AUTH_FAILED,
    BINKP_ERROR_PROTOCOL_ERROR
} ftn_binkp_error_t;

/* Binkp frame structure */
typedef struct {
    uint8_t header[2];
    uint8_t* data;
    size_t size;
    int is_command;
} ftn_binkp_frame_t;

/* Frame operations */
ftn_binkp_error_t ftn_binkp_frame_init(ftn_binkp_frame_t* frame);
void ftn_binkp_frame_free(ftn_binkp_frame_t* frame);
ftn_binkp_error_t ftn_binkp_frame_parse(const uint8_t* buffer, size_t len, ftn_binkp_frame_t* frame);
ftn_binkp_error_t ftn_binkp_frame_create(ftn_binkp_frame_t* frame, int is_command, const uint8_t* data, size_t size);
ftn_binkp_error_t ftn_binkp_frame_serialize(const ftn_binkp_frame_t* frame, uint8_t* buffer, size_t buffer_size, size_t* bytes_written);

/* Frame I/O operations */
ftn_binkp_error_t ftn_binkp_frame_send(ftn_net_connection_t* conn, const ftn_binkp_frame_t* frame);
ftn_binkp_error_t ftn_binkp_frame_receive(ftn_net_connection_t* conn, ftn_binkp_frame_t* frame, int timeout_ms);

/* Utility functions */
size_t ftn_binkp_frame_total_size(const ftn_binkp_frame_t* frame);
int ftn_binkp_frame_is_command(const ftn_binkp_frame_t* frame);
const char* ftn_binkp_error_string(ftn_binkp_error_t error);

#endif /* FTN_BINKP_H */