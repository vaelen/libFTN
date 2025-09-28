/*
 * bso.h - BinkleyTerm Style Outbound (BSO) management for libFTN
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

#ifndef FTN_BSO_H
#define FTN_BSO_H

#include <stddef.h>
#include <time.h>
#include "compat.h"

/* Forward declarations */
struct ftn_address;
struct ftn_flow_list;

/* BSO Error codes */
typedef enum {
    BSO_OK = 0,
    BSO_ERROR_INVALID_PATH,
    BSO_ERROR_PERMISSION,
    BSO_ERROR_NOT_FOUND,
    BSO_ERROR_INVALID_ADDRESS,
    BSO_ERROR_BUSY,
    BSO_ERROR_MEMORY,
    BSO_ERROR_FILE_IO
} ftn_bso_error_t;

/* BSO path structure */
typedef struct {
    char* base_path;
    char* domain;
    int zone;
    struct ftn_address* address;
} ftn_bso_path_t;

/* BSO directory entry */
typedef struct {
    char* filename;
    char* full_path;
    time_t mtime;
    size_t size;
    int is_directory;
} ftn_bso_entry_t;

/* BSO directory listing */
typedef struct {
    ftn_bso_entry_t* entries;
    size_t count;
    size_t capacity;
} ftn_bso_directory_t;

/* BSO path operations */
ftn_bso_error_t ftn_bso_path_init(ftn_bso_path_t* bso_path);
void ftn_bso_path_free(ftn_bso_path_t* bso_path);
ftn_bso_error_t ftn_bso_set_base_path(ftn_bso_path_t* bso_path, const char* base_path);
ftn_bso_error_t ftn_bso_set_domain(ftn_bso_path_t* bso_path, const char* domain);
ftn_bso_error_t ftn_bso_set_zone(ftn_bso_path_t* bso_path, int zone);
ftn_bso_error_t ftn_bso_set_address(ftn_bso_path_t* bso_path, const struct ftn_address* address);

/* BSO directory structure operations */
char* ftn_bso_get_outbound_path(const ftn_bso_path_t* bso_path);
char* ftn_bso_get_zone_path(const char* base_path, int zone);
char* ftn_bso_get_point_path(const char* base_path, const struct ftn_address* address);
ftn_bso_error_t ftn_bso_ensure_directory(const char* path);

/* Flow file naming */
char* ftn_bso_get_flow_filename(const struct ftn_address* addr, const char* flavor, const char* extension);
char* ftn_bso_address_to_hex(const struct ftn_address* addr);
ftn_bso_error_t ftn_bso_hex_to_address(const char* hex_str, struct ftn_address* addr);

/* Directory scanning */
ftn_bso_error_t ftn_bso_scan_directory(const char* path, ftn_bso_directory_t* directory);
ftn_bso_error_t ftn_bso_scan_outbound(const char* outbound_path, struct ftn_flow_list** flows);
void ftn_bso_directory_free(ftn_bso_directory_t* directory);

/* BSO validation */
int ftn_bso_is_valid_outbound(const char* path);
int ftn_bso_is_flow_file(const char* filename);
int ftn_bso_is_control_file(const char* filename);

/* Utility functions */
const char* ftn_bso_error_string(ftn_bso_error_t error);
ftn_bso_error_t ftn_bso_create_directories(const char* path);
time_t ftn_bso_get_file_mtime(const char* filepath);
size_t ftn_bso_get_file_size(const char* filepath);

/* File filtering */
typedef int (*ftn_bso_filter_func_t)(const char* filename, void* user_data);
ftn_bso_error_t ftn_bso_scan_filtered(const char* path, ftn_bso_filter_func_t filter, void* user_data, ftn_bso_directory_t* directory);

#endif /* FTN_BSO_H */