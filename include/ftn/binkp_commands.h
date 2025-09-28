/*
 * binkp_commands.h - Binkp command implementation for libFTN
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

#ifndef FTN_BINKP_COMMANDS_H
#define FTN_BINKP_COMMANDS_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "binkp.h"

/* Binkp command types */
typedef enum {
    BINKP_M_NUL = 0,
    BINKP_M_ADR = 1,
    BINKP_M_PWD = 2,
    BINKP_M_FILE = 3,
    BINKP_M_OK = 4,
    BINKP_M_EOB = 5,
    BINKP_M_GOT = 6,
    BINKP_M_ERR = 7,
    BINKP_M_BSY = 8,
    BINKP_M_GET = 9,
    BINKP_M_SKIP = 10
} ftn_binkp_command_t;

/* Command frame structure */
typedef struct {
    ftn_binkp_command_t cmd;
    char* args;
    size_t args_len;
} ftn_binkp_command_frame_t;

/* File transfer information */
typedef struct {
    char* filename;
    size_t file_size;
    time_t timestamp;
    size_t offset;
} ftn_binkp_file_info_t;

/* Command operations */
ftn_binkp_error_t ftn_binkp_command_init(ftn_binkp_command_frame_t* cmd_frame);
void ftn_binkp_command_free(ftn_binkp_command_frame_t* cmd_frame);

/* Command parsing */
ftn_binkp_error_t ftn_binkp_command_parse(const ftn_binkp_frame_t* frame, ftn_binkp_command_frame_t* cmd_frame);
ftn_binkp_error_t ftn_binkp_command_create(ftn_binkp_command_frame_t* cmd_frame, ftn_binkp_command_t cmd, const char* args);

/* Command frame conversion */
ftn_binkp_error_t ftn_binkp_command_to_frame(const ftn_binkp_command_frame_t* cmd_frame, ftn_binkp_frame_t* frame);

/* Specific command builders */
ftn_binkp_error_t ftn_binkp_create_m_nul(ftn_binkp_frame_t* frame, const char* info);
ftn_binkp_error_t ftn_binkp_create_m_adr(ftn_binkp_frame_t* frame, const char* addresses);
ftn_binkp_error_t ftn_binkp_create_m_pwd(ftn_binkp_frame_t* frame, const char* password);
ftn_binkp_error_t ftn_binkp_create_m_file(ftn_binkp_frame_t* frame, const ftn_binkp_file_info_t* file_info);
ftn_binkp_error_t ftn_binkp_create_m_ok(ftn_binkp_frame_t* frame, const char* info);
ftn_binkp_error_t ftn_binkp_create_m_eob(ftn_binkp_frame_t* frame);
ftn_binkp_error_t ftn_binkp_create_m_got(ftn_binkp_frame_t* frame, const char* filename, size_t bytes_received);
ftn_binkp_error_t ftn_binkp_create_m_err(ftn_binkp_frame_t* frame, const char* error_msg);
ftn_binkp_error_t ftn_binkp_create_m_bsy(ftn_binkp_frame_t* frame, const char* reason);
ftn_binkp_error_t ftn_binkp_create_m_get(ftn_binkp_frame_t* frame, const char* filename, size_t offset);
ftn_binkp_error_t ftn_binkp_create_m_skip(ftn_binkp_frame_t* frame, const char* filename, size_t offset);

/* Command argument parsing */
ftn_binkp_error_t ftn_binkp_parse_m_file(const ftn_binkp_command_frame_t* cmd_frame, ftn_binkp_file_info_t* file_info);
ftn_binkp_error_t ftn_binkp_parse_m_got(const ftn_binkp_command_frame_t* cmd_frame, char** filename, size_t* bytes_received);
ftn_binkp_error_t ftn_binkp_parse_m_get(const ftn_binkp_command_frame_t* cmd_frame, char** filename, size_t* offset);
ftn_binkp_error_t ftn_binkp_parse_m_skip(const ftn_binkp_command_frame_t* cmd_frame, char** filename, size_t* offset);

/* Utility functions */
const char* ftn_binkp_command_name(ftn_binkp_command_t cmd);
ftn_binkp_error_t ftn_binkp_escape_filename(const char* filename, char** escaped);
ftn_binkp_error_t ftn_binkp_unescape_filename(const char* escaped, char** filename);

/* File info operations */
ftn_binkp_error_t ftn_binkp_file_info_init(ftn_binkp_file_info_t* file_info);
void ftn_binkp_file_info_free(ftn_binkp_file_info_t* file_info);

#endif /* FTN_BINKP_COMMANDS_H */