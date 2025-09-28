/*
 * control.h - BSO Control file management for libFTN
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

#ifndef FTN_CONTROL_H
#define FTN_CONTROL_H

#include <stddef.h>
#include <time.h>
#include "bso.h"

/* Forward declarations */
struct ftn_address;

/* Control file types */
typedef enum {
    CONTROL_TYPE_BSY,       /* .bsy - busy file locking */
    CONTROL_TYPE_CSY,       /* .csy - call file coordination */
    CONTROL_TYPE_HLD,       /* .hld - hold file time-based holds */
    CONTROL_TYPE_TRY        /* .try - connection attempt tracking */
} ftn_control_type_t;

/* Control file structure */
typedef struct {
    struct ftn_address* address;
    char* control_path;
    ftn_control_type_t type;
    time_t created;
    time_t expires;
    char* pid_info;
    char* reason;
    int attempt_count;
} ftn_control_file_t;

/* Control file operations */
ftn_bso_error_t ftn_control_file_init(ftn_control_file_t* control);
void ftn_control_file_free(ftn_control_file_t* control);

/* BSY file operations (busy/locking) */
ftn_bso_error_t ftn_control_acquire_bsy(const struct ftn_address* addr, const char* outbound, ftn_control_file_t* control);
ftn_bso_error_t ftn_control_release_bsy(const ftn_control_file_t* control);
ftn_bso_error_t ftn_control_check_bsy(const struct ftn_address* addr, const char* outbound, ftn_control_file_t* control);

/* CSY file operations (call coordination) */
ftn_bso_error_t ftn_control_create_csy(const struct ftn_address* addr, const char* outbound, const char* info);
ftn_bso_error_t ftn_control_remove_csy(const struct ftn_address* addr, const char* outbound);
ftn_bso_error_t ftn_control_check_csy(const struct ftn_address* addr, const char* outbound, ftn_control_file_t* control);

/* HLD file operations (hold management) */
ftn_bso_error_t ftn_control_create_hld(const struct ftn_address* addr, const char* outbound, time_t until, const char* reason);
ftn_bso_error_t ftn_control_remove_hld(const struct ftn_address* addr, const char* outbound);
ftn_bso_error_t ftn_control_check_hld(const struct ftn_address* addr, const char* outbound, time_t* until, char** reason);

/* TRY file operations (attempt tracking) */
ftn_bso_error_t ftn_control_create_try(const struct ftn_address* addr, const char* outbound, const char* reason);
ftn_bso_error_t ftn_control_update_try(const struct ftn_address* addr, const char* outbound, int attempt_count);
ftn_bso_error_t ftn_control_remove_try(const struct ftn_address* addr, const char* outbound);
ftn_bso_error_t ftn_control_check_try(const struct ftn_address* addr, const char* outbound, ftn_control_file_t* control);

/* Control file utilities */
char* ftn_control_get_filename(const struct ftn_address* addr, ftn_control_type_t type);
char* ftn_control_get_filepath(const struct ftn_address* addr, const char* outbound, ftn_control_type_t type);
ftn_bso_error_t ftn_control_parse_filename(const char* filename, struct ftn_address* addr, ftn_control_type_t* type);

/* Control file validation and cleanup */
int ftn_control_is_stale(const ftn_control_file_t* control, time_t max_age);
ftn_bso_error_t ftn_control_cleanup_stale(const char* outbound, time_t max_age);
ftn_bso_error_t ftn_control_scan_directory(const char* outbound, ftn_control_file_t** controls, size_t* count);

/* Safe file operations */
ftn_bso_error_t ftn_control_atomic_create(const char* filepath, const char* content);
ftn_bso_error_t ftn_control_atomic_remove(const char* filepath);
ftn_bso_error_t ftn_control_read_content(const char* filepath, char** content);

/* Control file content parsing */
ftn_bso_error_t ftn_control_parse_bsy_content(const char* content, char** pid_info);
ftn_bso_error_t ftn_control_parse_hld_content(const char* content, time_t* until, char** reason);
ftn_bso_error_t ftn_control_parse_try_content(const char* content, int* attempt_count, char** reason);

/* Control file content generation */
ftn_bso_error_t ftn_control_generate_bsy_content(char** content);
ftn_bso_error_t ftn_control_generate_hld_content(time_t until, const char* reason, char** content);
ftn_bso_error_t ftn_control_generate_try_content(int attempt_count, const char* reason, char** content);

/* Utility functions */
const char* ftn_control_type_string(ftn_control_type_t type);
const char* ftn_control_type_extension(ftn_control_type_t type);
int ftn_control_type_from_extension(const char* extension, ftn_control_type_t* type);

/* Lock management */
typedef struct {
    struct ftn_address* address;
    char* outbound_path;
    ftn_control_file_t* bsy_file;
    time_t lock_time;
} ftn_control_lock_t;

ftn_bso_error_t ftn_control_lock_init(ftn_control_lock_t* lock);
void ftn_control_lock_free(ftn_control_lock_t* lock);
ftn_bso_error_t ftn_control_acquire_lock(const struct ftn_address* addr, const char* outbound, ftn_control_lock_t* lock);
ftn_bso_error_t ftn_control_release_lock(ftn_control_lock_t* lock);

#endif /* FTN_CONTROL_H */