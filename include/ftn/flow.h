/*
 * flow.h - BSO Flow file processing for libFTN
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

#ifndef FTN_FLOW_H
#define FTN_FLOW_H

#include <stddef.h>
#include <time.h>
#include "bso.h"

/* Forward declarations */
struct ftn_address;

/* Flow file types */
typedef enum {
    FLOW_TYPE_NETMAIL,      /* .?ut files */
    FLOW_TYPE_REFERENCE     /* .?lo files */
} ftn_flow_type_t;

/* Flow file flavors (priority order) */
typedef enum {
    FLOW_FLAVOR_IMMEDIATE,  /* i?? - highest priority */
    FLOW_FLAVOR_CONTINUOUS, /* c?? - honor internal restrictions only */
    FLOW_FLAVOR_DIRECT,     /* d?? - honor all restrictions */
    FLOW_FLAVOR_NORMAL,     /* out/flo - standard processing */
    FLOW_FLAVOR_HOLD        /* h?? - wait for remote poll */
} ftn_flow_flavor_t;

/* Reference file directive types */
typedef enum {
    REF_DIRECTIVE_NONE,     /* No directive - send only */
    REF_DIRECTIVE_TRUNCATE, /* # - truncate after send */
    REF_DIRECTIVE_DELETE,   /* ^ or - - delete after send */
    REF_DIRECTIVE_SKIP,     /* ~ or ! - skip processing */
    REF_DIRECTIVE_SEND      /* @ - explicit send directive */
} ftn_ref_directive_t;

/* Reference file entry */
typedef struct {
    char* filepath;
    ftn_ref_directive_t directive;
    int processed;
    time_t timestamp;
    size_t file_size;
} ftn_reference_entry_t;

/* Flow file structure */
typedef struct {
    char* filepath;
    char* filename;
    ftn_flow_type_t type;
    ftn_flow_flavor_t flavor;
    struct ftn_address* target_address;
    time_t timestamp;
    size_t file_count;
    ftn_reference_entry_t* entries;
    size_t entry_capacity;
} ftn_flow_file_t;

/* Flow file list */
typedef struct {
    ftn_flow_file_t* flows;
    size_t count;
    size_t capacity;
} ftn_flow_list_t;

/* Flow file operations */
ftn_bso_error_t ftn_flow_file_init(ftn_flow_file_t* flow);
void ftn_flow_file_free(ftn_flow_file_t* flow);
ftn_bso_error_t ftn_flow_parse_filename(const char* filename, ftn_flow_type_t* type, ftn_flow_flavor_t* flavor, struct ftn_address* address);

/* Flow list operations */
ftn_bso_error_t ftn_flow_list_init(ftn_flow_list_t* list);
void ftn_flow_list_free(ftn_flow_list_t* list);
ftn_bso_error_t ftn_flow_list_add(ftn_flow_list_t* list, const ftn_flow_file_t* flow);
ftn_bso_error_t ftn_flow_list_sort_by_priority(ftn_flow_list_t* list);

/* Flow file processing */
ftn_bso_error_t ftn_flow_load_file(const char* filepath, ftn_flow_file_t* flow);
ftn_bso_error_t ftn_flow_parse_reference_file(const char* filepath, ftn_flow_file_t* flow);
ftn_bso_error_t ftn_flow_process_netmail_file(const char* filepath, ftn_flow_file_t* flow);

/* Reference entry operations */
ftn_bso_error_t ftn_flow_parse_reference_line(const char* line, ftn_reference_entry_t* entry);
ftn_bso_error_t ftn_flow_add_reference_entry(ftn_flow_file_t* flow, const ftn_reference_entry_t* entry);
void ftn_flow_reference_entry_free(ftn_reference_entry_t* entry);

/* Flow file discovery */
ftn_bso_error_t ftn_flow_scan_outbound(const char* outbound_path, ftn_flow_list_t* flows);
ftn_bso_error_t ftn_flow_scan_zone_directory(const char* zone_path, int zone, ftn_flow_list_t* flows);
ftn_bso_error_t ftn_flow_scan_point_directory(const char* point_path, const struct ftn_address* base_addr, ftn_flow_list_t* flows);

/* Flow file filtering */
typedef int (*ftn_flow_filter_func_t)(const ftn_flow_file_t* flow, void* user_data);
ftn_bso_error_t ftn_flow_list_filter(ftn_flow_list_t* list, ftn_flow_filter_func_t filter, void* user_data);

/* Flow file validation */
int ftn_flow_is_valid_reference_file(const char* filepath);
int ftn_flow_is_valid_netmail_file(const char* filepath);
ftn_bso_error_t ftn_flow_validate_file_exists(const ftn_reference_entry_t* entry);

/* Flow file utilities */
const char* ftn_flow_type_string(ftn_flow_type_t type);
const char* ftn_flow_flavor_string(ftn_flow_flavor_t flavor);
const char* ftn_flow_directive_string(ftn_ref_directive_t directive);
int ftn_flow_flavor_priority(ftn_flow_flavor_t flavor);

/* Flow file modification */
ftn_bso_error_t ftn_flow_mark_entry_processed(ftn_flow_file_t* flow, size_t entry_index);
ftn_bso_error_t ftn_flow_remove_processed_entries(ftn_flow_file_t* flow);
ftn_bso_error_t ftn_flow_update_file(const ftn_flow_file_t* flow);

/* Filename pattern matching */
int ftn_flow_matches_pattern(const char* filename, const char* pattern);
ftn_bso_error_t ftn_flow_generate_filename(const struct ftn_address* addr, ftn_flow_type_t type, ftn_flow_flavor_t flavor, char** filename);

#endif /* FTN_FLOW_H */