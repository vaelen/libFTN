/*
 * dupecheck.h - Duplicate detection system for libFTN
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

#ifndef FTN_DUPECHECK_H
#define FTN_DUPECHECK_H

#include "ftn.h"
#include "ftn/packet.h"
#include <time.h>

/* Duplicate checker structure */
typedef struct {
    char* db_path;                    /* Path to duplicate database file */
    void* db_handle;                  /* Internal database handle */
    time_t retention_days;            /* How long to keep records (in seconds) */
    size_t max_entries;               /* Maximum number of entries */
} ftn_dupecheck_t;

/* Statistics structure */
typedef struct {
    size_t total_entries;             /* Total messages in database */
    size_t entries_cleaned;           /* Entries removed in last cleanup */
    time_t last_cleanup;              /* Last cleanup timestamp */
    time_t oldest_entry;              /* Oldest message timestamp */
    size_t lookups_performed;         /* Number of lookups performed */
    size_t duplicates_found;          /* Number of duplicates detected */
} ftn_dupecheck_stats_t;

/* Database entry structure (internal) */
typedef struct {
    char* msgid;                      /* Normalized MSGID */
    time_t timestamp;                 /* When message was first seen */
} ftn_dupecheck_entry_t;

/* Database structure (internal) */
typedef struct {
    ftn_dupecheck_entry_t* entries;   /* Array of entries */
    size_t entry_count;               /* Number of entries */
    size_t entry_capacity;            /* Allocated capacity */
    int modified;                     /* Whether database needs saving */
} ftn_dupecheck_db_t;

/* Duplicate checker lifecycle */
ftn_dupecheck_t* ftn_dupecheck_new(const char* db_path);
void ftn_dupecheck_free(ftn_dupecheck_t* dupecheck);
ftn_error_t ftn_dupecheck_load(ftn_dupecheck_t* dupecheck);
ftn_error_t ftn_dupecheck_save(ftn_dupecheck_t* dupecheck);

/* Duplicate detection operations */
ftn_error_t ftn_dupecheck_is_duplicate(ftn_dupecheck_t* dupecheck, const ftn_message_t* msg, int* is_dupe);
ftn_error_t ftn_dupecheck_add_message(ftn_dupecheck_t* dupecheck, const ftn_message_t* msg);

/* MSGID operations */
char* ftn_dupecheck_extract_msgid(const ftn_message_t* msg);
char* ftn_dupecheck_normalize_msgid(const char* msgid);

/* Maintenance operations */
ftn_error_t ftn_dupecheck_cleanup_old(ftn_dupecheck_t* dupecheck, time_t cutoff_time);
ftn_error_t ftn_dupecheck_get_stats(const ftn_dupecheck_t* dupecheck, ftn_dupecheck_stats_t* stats);
ftn_error_t ftn_dupecheck_set_retention(ftn_dupecheck_t* dupecheck, time_t retention_days);
ftn_error_t ftn_dupecheck_set_max_entries(ftn_dupecheck_t* dupecheck, size_t max_entries);

/* Utility functions */
int ftn_dupecheck_is_valid_msgid(const char* msgid);
time_t ftn_dupecheck_parse_timestamp(const char* timestamp_str);
char* ftn_dupecheck_format_timestamp(time_t timestamp);

/* Database functions (internal) */
ftn_dupecheck_db_t* ftn_dupecheck_db_new(void);
void ftn_dupecheck_db_free(ftn_dupecheck_db_t* db);
ftn_error_t ftn_dupecheck_db_load(ftn_dupecheck_db_t* db, const char* db_path);
ftn_error_t ftn_dupecheck_db_save(ftn_dupecheck_db_t* db, const char* db_path);
ftn_error_t ftn_dupecheck_db_add_entry(ftn_dupecheck_db_t* db, const char* msgid, time_t timestamp);
int ftn_dupecheck_db_find_entry(const ftn_dupecheck_db_t* db, const char* msgid);
ftn_error_t ftn_dupecheck_db_cleanup_old(ftn_dupecheck_db_t* db, time_t cutoff_time);

#endif /* FTN_DUPECHECK_H */