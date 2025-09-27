/*
 * storage.h - Storage implementation for libFTN
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

#ifndef FTN_STORAGE_H
#define FTN_STORAGE_H

#include "ftn.h"
#include "ftn/packet.h"
#include "ftn/config.h"
#include "ftn/rfc822.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Storage system structure */
typedef struct {
    const ftn_config_t* config;
    char* news_root;             /* Base news spool directory */
    char* mail_root;             /* Base mail directory */
    FILE* active_file;           /* Active file handle */
    char* active_file_path;      /* Path to active file */
} ftn_storage_t;

/* Message list structure for outbound scanning */
typedef struct {
    ftn_message_t** messages;    /* Array of message pointers */
    size_t count;                /* Number of messages */
    size_t capacity;             /* Allocated capacity */
} ftn_message_list_t;

/* Newsgroup information structure */
typedef struct {
    char* newsgroup;             /* Newsgroup name */
    long first_article;          /* First article number */
    long last_article;           /* Last article number */
    char status;                 /* Group status (y/n/m/x) */
} ftn_newsgroup_info_t;

/* Maildir filename information */
typedef struct {
    char* filename;              /* Generated filename */
    char* tmp_path;              /* Full path in tmp directory */
    char* new_path;              /* Full path in new directory */
} ftn_maildir_file_t;

/* Storage system lifecycle */
ftn_storage_t* ftn_storage_new(const ftn_config_t* config);
void ftn_storage_free(ftn_storage_t* storage);
ftn_error_t ftn_storage_initialize(ftn_storage_t* storage);

/* Maildir operations */
ftn_error_t ftn_storage_store_mail(ftn_storage_t* storage, const ftn_message_t* msg,
                                  const char* username, const char* network);
ftn_error_t ftn_storage_create_maildir(const char* path);
ftn_error_t ftn_storage_generate_maildir_filename(ftn_maildir_file_t* file_info,
                                                 const char* maildir_path);
void ftn_maildir_file_free(ftn_maildir_file_t* file_info);

/* USENET spool operations */
ftn_error_t ftn_storage_store_news(ftn_storage_t* storage, const ftn_message_t* msg,
                                  const char* area, const char* network);
ftn_error_t ftn_storage_create_newsgroup(ftn_storage_t* storage, const char* newsgroup);
ftn_error_t ftn_storage_update_active_file(ftn_storage_t* storage, const char* newsgroup,
                                          long article_num);
ftn_error_t ftn_storage_get_next_article_number(ftn_storage_t* storage, const char* newsgroup,
                                               long* article_num);

/* Outbound message scanning */
ftn_error_t ftn_storage_scan_outbound_mail(ftn_storage_t* storage, const char* username,
                                          const char* network, ftn_message_list_t* messages);
ftn_error_t ftn_storage_scan_outbound_news(ftn_storage_t* storage, const char* area,
                                          const char* network, ftn_message_list_t* messages);

/* Message list utilities */
ftn_message_list_t* ftn_message_list_new(void);
void ftn_message_list_free(ftn_message_list_t* list);
ftn_error_t ftn_message_list_add(ftn_message_list_t* list, ftn_message_t* message);
ftn_error_t ftn_message_list_clear(ftn_message_list_t* list);

/* Newsgroup info utilities */
ftn_newsgroup_info_t* ftn_newsgroup_info_new(void);
void ftn_newsgroup_info_free(ftn_newsgroup_info_t* info);
ftn_error_t ftn_newsgroup_info_set(ftn_newsgroup_info_t* info, const char* newsgroup,
                                  long first, long last, char status);

/* Active file management */
ftn_error_t ftn_storage_load_active_file(ftn_storage_t* storage);
ftn_error_t ftn_storage_save_active_file(ftn_storage_t* storage);
ftn_error_t ftn_storage_find_newsgroup_info(ftn_storage_t* storage, const char* newsgroup,
                                           ftn_newsgroup_info_t* info);

/* Utility functions */
char* ftn_storage_expand_path(const char* template, const char* username, const char* network);
ftn_error_t ftn_storage_ensure_directory(const char* path, mode_t mode);
ftn_error_t ftn_storage_write_file_atomic(const char* path, const char* content, size_t length);
ftn_error_t ftn_storage_validate_path(const char* path);

/* Path sanitization and security */
char* ftn_storage_sanitize_filename(const char* input);
char* ftn_storage_sanitize_username(const char* username);
char* ftn_storage_sanitize_area_name(const char* area);
int ftn_storage_is_safe_path(const char* path);

/* File system utilities */
int ftn_storage_file_exists(const char* path);
int ftn_storage_directory_exists(const char* path);
ftn_error_t ftn_storage_create_directory_recursive(const char* path, mode_t mode);
ftn_error_t ftn_storage_get_unique_filename(const char* directory, const char* prefix,
                                           const char* suffix, char** filename);

/* Advanced filename generation */
ftn_error_t ftn_storage_generate_message_filename(const ftn_message_t* msg, char** filename);

/* Message duplicate detection */
ftn_error_t ftn_storage_message_exists(ftn_storage_t* storage, const char* maildir_path,
                                      const ftn_message_t* msg, const char* filename, int* exists);

/* Conversion integration */
ftn_error_t ftn_storage_convert_to_rfc822(const ftn_message_t* ftn_msg, const char* domain,
                                         char** rfc822_text);
ftn_error_t ftn_storage_convert_to_usenet(const ftn_message_t* ftn_msg, const char* network,
                                         char** usenet_text);
ftn_error_t ftn_storage_convert_from_rfc822(const char* rfc822_text, const char* domain,
                                           ftn_message_t** ftn_msg);
ftn_error_t ftn_storage_convert_from_usenet(const char* usenet_text, const char* network,
                                           ftn_message_t** ftn_msg);

/* Utility-compatible storage functions */
ftn_error_t ftn_storage_store_mail_with_options(ftn_storage_t* storage, const ftn_message_t* msg,
                                               const char* maildir_path, const char* domain,
                                               const char* user_filter);

ftn_error_t ftn_storage_store_news_with_options(ftn_storage_t* storage, const ftn_message_t* msg,
                                               const char* usenet_root, const char* network);

/* Simple storage functions that don't require full storage context */
ftn_error_t ftn_storage_store_mail_simple(const ftn_message_t* msg, const char* maildir_path,
                                         const char* domain, const char* user_filter);

ftn_error_t ftn_storage_store_news_simple(const ftn_message_t* msg, const char* usenet_root,
                                         const char* network);

/* Lock file operations for atomic updates */
ftn_error_t ftn_storage_acquire_lock(const char* lock_path, int* lock_fd);
ftn_error_t ftn_storage_release_lock(int lock_fd, const char* lock_path);

/* Default file permissions */
#define FTN_STORAGE_DIR_MODE  (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)  /* 0755 */
#define FTN_STORAGE_FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)           /* 0644 */

/* Maildir subdirectories */
#define FTN_MAILDIR_TMP "tmp"
#define FTN_MAILDIR_NEW "new"
#define FTN_MAILDIR_CUR "cur"

/* USENET active file name */
#define FTN_USENET_ACTIVE_FILE "active"

#endif /* FTN_STORAGE_H */