/*
 * dupecheck.c - Duplicate detection system implementation
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "ftn.h"
#include "ftn/dupecheck.h"
#include "ftn/packet.h"

/* Default settings */
#define DEFAULT_RETENTION_DAYS (30 * 24 * 60 * 60)  /* 30 days in seconds */
#define DEFAULT_MAX_ENTRIES    10000
#define DB_VERSION_STRING      "# libFTN Duplicate Database v1.0"

/* Utility functions */
char* ftn_dupecheck_strdup(const char* str) {
    char* result;
    if (!str) return NULL;

    result = malloc(strlen(str) + 1);
    if (result) {
        strcpy(result, str);
    }
    return result;
}

void ftn_dupecheck_trim(char* str) {
    char *start, *end;

    if (!str) return;

    /* Trim leading whitespace */
    start = str;
    while (isspace((unsigned char)*start)) start++;

    /* Trim trailing whitespace */
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;

    /* Move string to beginning and null terminate */
    memmove(str, start, end - start + 1);
    str[end - start + 1] = '\0';
}

/* MSGID extraction and normalization */
char* ftn_dupecheck_extract_msgid(const ftn_message_t* msg) {
    size_t i;
    const char* msgid_prefix = "MSGID:";
    const char* line;
    char* msgid_line;
    char* msgid_start;

    if (!msg || !msg->control_lines) {
        return NULL;
    }

    /* Search through control lines for MSGID */
    for (i = 0; i < msg->control_count; i++) {
        line = msg->control_lines[i];
        if (!line) continue;

        /* Check if line starts with MSGID: (case insensitive) */
        if (strncasecmp(line, msgid_prefix, strlen(msgid_prefix)) == 0) {
            msgid_line = ftn_dupecheck_strdup(line);
            if (!msgid_line) return NULL;

            /* Find the start of the actual MSGID value */
            msgid_start = msgid_line + strlen(msgid_prefix);
            while (*msgid_start && isspace((unsigned char)*msgid_start)) {
                msgid_start++;
            }

            /* Trim the MSGID value */
            ftn_dupecheck_trim(msgid_start);

            if (strlen(msgid_start) > 0) {
                char* result = ftn_dupecheck_strdup(msgid_start);
                free(msgid_line);
                return result;
            }

            free(msgid_line);
        }
    }

    return NULL;
}

char* ftn_dupecheck_normalize_msgid(const char* msgid) {
    char* normalized;
    char* src;
    char* dst;

    if (!msgid) return NULL;

    normalized = ftn_dupecheck_strdup(msgid);
    if (!normalized) return NULL;

    /* Normalize whitespace and convert to lowercase */
    src = normalized;
    dst = normalized;

    while (*src) {
        if (isspace((unsigned char)*src)) {
            /* Collapse multiple whitespace to single space */
            *dst++ = ' ';
            while (*src && isspace((unsigned char)*src)) src++;
        } else {
            *dst++ = tolower((unsigned char)*src++);
        }
    }

    *dst = '\0';

    /* Trim any trailing whitespace */
    ftn_dupecheck_trim(normalized);

    return normalized;
}

int ftn_dupecheck_is_valid_msgid(const char* msgid) {
    if (!msgid || strlen(msgid) == 0) {
        return 0;
    }

    /* MSGID should contain at least one non-whitespace character */
    while (*msgid) {
        if (!isspace((unsigned char)*msgid)) {
            return 1;
        }
        msgid++;
    }

    return 0;
}

/* Database functions */
ftn_dupecheck_db_t* ftn_dupecheck_db_new(void) {
    ftn_dupecheck_db_t* db = malloc(sizeof(ftn_dupecheck_db_t));
    if (!db) return NULL;

    db->entries = NULL;
    db->entry_count = 0;
    db->entry_capacity = 0;
    db->modified = 0;

    return db;
}

void ftn_dupecheck_db_free(ftn_dupecheck_db_t* db) {
    size_t i;

    if (!db) return;

    if (db->entries) {
        for (i = 0; i < db->entry_count; i++) {
            if (db->entries[i].msgid) {
                free(db->entries[i].msgid);
            }
        }
        free(db->entries);
    }

    free(db);
}

ftn_error_t ftn_dupecheck_db_add_entry(ftn_dupecheck_db_t* db, const char* msgid, time_t timestamp) {
    ftn_dupecheck_entry_t* new_entries;

    if (!db || !msgid) return FTN_ERROR_INVALID_PARAMETER;

    /* Check if entry already exists */
    if (ftn_dupecheck_db_find_entry(db, msgid) >= 0) {
        return FTN_OK; /* Already exists, no need to add */
    }

    /* Grow array if needed */
    if (db->entry_count >= db->entry_capacity) {
        size_t new_capacity = db->entry_capacity ? db->entry_capacity * 2 : 16;
        new_entries = realloc(db->entries, new_capacity * sizeof(ftn_dupecheck_entry_t));
        if (!new_entries) return FTN_ERROR_NOMEM;

        db->entries = new_entries;
        db->entry_capacity = new_capacity;
    }

    /* Add new entry */
    db->entries[db->entry_count].msgid = ftn_dupecheck_strdup(msgid);
    if (!db->entries[db->entry_count].msgid) return FTN_ERROR_NOMEM;

    db->entries[db->entry_count].timestamp = timestamp;
    db->entry_count++;
    db->modified = 1;

    return FTN_OK;
}

int ftn_dupecheck_db_find_entry(const ftn_dupecheck_db_t* db, const char* msgid) {
    size_t i;

    if (!db || !msgid) return -1;

    for (i = 0; i < db->entry_count; i++) {
        if (db->entries[i].msgid && strcmp(db->entries[i].msgid, msgid) == 0) {
            return (int)i;
        }
    }

    return -1;
}

ftn_error_t ftn_dupecheck_db_cleanup_old(ftn_dupecheck_db_t* db, time_t cutoff_time) {
    size_t i, j;
    size_t entries_removed = 0;

    if (!db) return FTN_ERROR_INVALID_PARAMETER;

    /* Remove entries older than cutoff time */
    for (i = 0; i < db->entry_count; ) {
        if (db->entries[i].timestamp < cutoff_time) {
            /* Free the MSGID string */
            if (db->entries[i].msgid) {
                free(db->entries[i].msgid);
            }

            /* Shift remaining entries down */
            for (j = i; j < db->entry_count - 1; j++) {
                db->entries[j] = db->entries[j + 1];
            }

            db->entry_count--;
            entries_removed++;
            db->modified = 1;
        } else {
            i++;
        }
    }

    return FTN_OK;
}

time_t ftn_dupecheck_parse_timestamp(const char* timestamp_str) {
    long timestamp_long;

    if (!timestamp_str) return 0;

    timestamp_long = strtol(timestamp_str, NULL, 10);
    return (time_t)timestamp_long;
}

char* ftn_dupecheck_format_timestamp(time_t timestamp) {
    char* result = malloc(32);
    if (!result) return NULL;

    snprintf(result, 32, "%ld", (long)timestamp);
    return result;
}

ftn_error_t ftn_dupecheck_db_load(ftn_dupecheck_db_t* db, const char* db_path) {
    FILE* fp;
    char line[1024];
    char* pipe_pos;
    char* timestamp_str;
    char* msgid_str;
    time_t timestamp;

    if (!db || !db_path) return FTN_ERROR_INVALID_PARAMETER;

    fp = fopen(db_path, "r");
    if (!fp) {
        /* File doesn't exist yet, that's OK for a new database */
        return FTN_OK;
    }

    while (fgets(line, sizeof(line), fp)) {
        ftn_dupecheck_trim(line);

        /* Skip empty lines and comments */
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }

        /* Parse line: timestamp|msgid */
        pipe_pos = strchr(line, '|');
        if (!pipe_pos) {
            continue; /* Invalid line format */
        }

        *pipe_pos = '\0';
        timestamp_str = line;
        msgid_str = pipe_pos + 1;

        ftn_dupecheck_trim(timestamp_str);
        ftn_dupecheck_trim(msgid_str);

        timestamp = ftn_dupecheck_parse_timestamp(timestamp_str);
        if (timestamp > 0 && strlen(msgid_str) > 0) {
            ftn_dupecheck_db_add_entry(db, msgid_str, timestamp);
        }
    }

    fclose(fp);
    db->modified = 0; /* Clear modified flag after loading */
    return FTN_OK;
}

ftn_error_t ftn_dupecheck_db_save(ftn_dupecheck_db_t* db, const char* db_path) {
    FILE* fp;
    size_t i;
    char* timestamp_str;

    if (!db || !db_path) return FTN_ERROR_INVALID_PARAMETER;

    if (!db->modified) {
        return FTN_OK; /* No changes to save */
    }

    fp = fopen(db_path, "w");
    if (!fp) return FTN_ERROR_FILE;

    /* Write header */
    fprintf(fp, "%s\n", DB_VERSION_STRING);
    fprintf(fp, "# timestamp|msgid\n");

    /* Write entries */
    for (i = 0; i < db->entry_count; i++) {
        if (db->entries[i].msgid) {
            timestamp_str = ftn_dupecheck_format_timestamp(db->entries[i].timestamp);
            if (timestamp_str) {
                fprintf(fp, "%s|%s\n", timestamp_str, db->entries[i].msgid);
                free(timestamp_str);
            }
        }
    }

    fclose(fp);
    db->modified = 0; /* Clear modified flag after saving */
    return FTN_OK;
}

/* Main dupecheck functions */
ftn_dupecheck_t* ftn_dupecheck_new(const char* db_path) {
    ftn_dupecheck_t* dupecheck;

    if (!db_path) return NULL;

    dupecheck = malloc(sizeof(ftn_dupecheck_t));
    if (!dupecheck) return NULL;

    dupecheck->db_path = ftn_dupecheck_strdup(db_path);
    if (!dupecheck->db_path) {
        free(dupecheck);
        return NULL;
    }

    dupecheck->db_handle = ftn_dupecheck_db_new();
    if (!dupecheck->db_handle) {
        free(dupecheck->db_path);
        free(dupecheck);
        return NULL;
    }

    dupecheck->retention_days = DEFAULT_RETENTION_DAYS;
    dupecheck->max_entries = DEFAULT_MAX_ENTRIES;

    return dupecheck;
}

void ftn_dupecheck_free(ftn_dupecheck_t* dupecheck) {
    if (!dupecheck) return;

    if (dupecheck->db_path) {
        free(dupecheck->db_path);
    }

    if (dupecheck->db_handle) {
        ftn_dupecheck_db_free((ftn_dupecheck_db_t*)dupecheck->db_handle);
    }

    free(dupecheck);
}

ftn_error_t ftn_dupecheck_load(ftn_dupecheck_t* dupecheck) {
    if (!dupecheck || !dupecheck->db_handle) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    return ftn_dupecheck_db_load((ftn_dupecheck_db_t*)dupecheck->db_handle, dupecheck->db_path);
}

ftn_error_t ftn_dupecheck_save(ftn_dupecheck_t* dupecheck) {
    if (!dupecheck || !dupecheck->db_handle) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    return ftn_dupecheck_db_save((ftn_dupecheck_db_t*)dupecheck->db_handle, dupecheck->db_path);
}

ftn_error_t ftn_dupecheck_is_duplicate(ftn_dupecheck_t* dupecheck, const ftn_message_t* msg, int* is_dupe) {
    char* msgid;
    char* normalized_msgid;
    ftn_dupecheck_db_t* db;
    int found;

    if (!dupecheck || !msg || !is_dupe) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    *is_dupe = 0;
    db = (ftn_dupecheck_db_t*)dupecheck->db_handle;

    /* Extract MSGID from message */
    msgid = ftn_dupecheck_extract_msgid(msg);
    if (!msgid) {
        /* No MSGID found, not a duplicate */
        return FTN_OK;
    }

    /* Normalize MSGID */
    normalized_msgid = ftn_dupecheck_normalize_msgid(msgid);
    free(msgid);

    if (!normalized_msgid || !ftn_dupecheck_is_valid_msgid(normalized_msgid)) {
        if (normalized_msgid) free(normalized_msgid);
        return FTN_OK;
    }

    /* Search for MSGID in database */
    found = ftn_dupecheck_db_find_entry(db, normalized_msgid);
    *is_dupe = (found >= 0) ? 1 : 0;

    free(normalized_msgid);
    return FTN_OK;
}

ftn_error_t ftn_dupecheck_add_message(ftn_dupecheck_t* dupecheck, const ftn_message_t* msg) {
    char* msgid;
    char* normalized_msgid;
    ftn_dupecheck_db_t* db;
    ftn_error_t result;
    time_t current_time;

    if (!dupecheck || !msg) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    db = (ftn_dupecheck_db_t*)dupecheck->db_handle;

    /* Extract MSGID from message */
    msgid = ftn_dupecheck_extract_msgid(msg);
    if (!msgid) {
        /* No MSGID found, nothing to add */
        return FTN_OK;
    }

    /* Normalize MSGID */
    normalized_msgid = ftn_dupecheck_normalize_msgid(msgid);
    free(msgid);

    if (!normalized_msgid || !ftn_dupecheck_is_valid_msgid(normalized_msgid)) {
        if (normalized_msgid) free(normalized_msgid);
        return FTN_OK;
    }

    /* Add to database with current timestamp */
    time(&current_time);
    result = ftn_dupecheck_db_add_entry(db, normalized_msgid, current_time);

    free(normalized_msgid);
    return result;
}

ftn_error_t ftn_dupecheck_cleanup_old(ftn_dupecheck_t* dupecheck, time_t cutoff_time) {
    if (!dupecheck || !dupecheck->db_handle) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    return ftn_dupecheck_db_cleanup_old((ftn_dupecheck_db_t*)dupecheck->db_handle, cutoff_time);
}

ftn_error_t ftn_dupecheck_get_stats(const ftn_dupecheck_t* dupecheck, ftn_dupecheck_stats_t* stats) {
    ftn_dupecheck_db_t* db;
    size_t i;
    time_t oldest = 0;

    if (!dupecheck || !stats) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    db = (ftn_dupecheck_db_t*)dupecheck->db_handle;

    memset(stats, 0, sizeof(ftn_dupecheck_stats_t));
    stats->total_entries = db->entry_count;

    /* Find oldest entry */
    for (i = 0; i < db->entry_count; i++) {
        if (oldest == 0 || db->entries[i].timestamp < oldest) {
            oldest = db->entries[i].timestamp;
        }
    }

    stats->oldest_entry = oldest;
    time(&stats->last_cleanup); /* For now, just use current time */

    return FTN_OK;
}

ftn_error_t ftn_dupecheck_set_retention(ftn_dupecheck_t* dupecheck, time_t retention_days) {
    if (!dupecheck) return FTN_ERROR_INVALID_PARAMETER;

    dupecheck->retention_days = retention_days;
    return FTN_OK;
}

ftn_error_t ftn_dupecheck_set_max_entries(ftn_dupecheck_t* dupecheck, size_t max_entries) {
    if (!dupecheck) return FTN_ERROR_INVALID_PARAMETER;

    dupecheck->max_entries = max_entries;
    return FTN_OK;
}