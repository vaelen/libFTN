/*
 * storage.c - Storage implementation for libFTN
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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

#include "ftn.h"
#include "ftn/storage.h"
#include "ftn/config.h"
#include "ftn/packet.h"
#include "ftn/rfc822.h"

/* Internal utility functions */
static char* ftn_storage_strdup(const char* str) {
    char* result;
    if (!str) return NULL;

    result = malloc(strlen(str) + 1);
    if (result) {
        strcpy(result, str);
    }
    return result;
}

static void ftn_storage_safe_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

/* Storage system lifecycle */
ftn_storage_t* ftn_storage_new(const ftn_config_t* config) {
    ftn_storage_t* storage;
    const ftn_news_config_t* news_config;
    const ftn_mail_config_t* mail_config;

    if (!config) {
        return NULL;
    }

    storage = malloc(sizeof(ftn_storage_t));
    if (!storage) {
        return NULL;
    }

    memset(storage, 0, sizeof(ftn_storage_t));
    storage->config = config;

    /* Get news and mail configuration */
    news_config = ftn_config_get_news(config);
    mail_config = ftn_config_get_mail(config);

    if (news_config && news_config->path) {
        storage->news_root = ftn_storage_strdup(news_config->path);
        if (!storage->news_root) {
            ftn_storage_free(storage);
            return NULL;
        }

        /* Set up active file path */
        storage->active_file_path = malloc(strlen(storage->news_root) + strlen(FTN_USENET_ACTIVE_FILE) + 2);
        if (storage->active_file_path) {
            sprintf(storage->active_file_path, "%s/%s", storage->news_root, FTN_USENET_ACTIVE_FILE);
        }
    }

    if (mail_config && mail_config->inbox) {
        storage->mail_root = ftn_storage_strdup(mail_config->inbox);
        if (!storage->mail_root) {
            ftn_storage_free(storage);
            return NULL;
        }
    }

    return storage;
}

void ftn_storage_free(ftn_storage_t* storage) {
    if (!storage) return;

    if (storage->active_file) {
        fclose(storage->active_file);
    }

    ftn_storage_safe_free(storage->news_root);
    ftn_storage_safe_free(storage->mail_root);
    ftn_storage_safe_free(storage->active_file_path);
    free(storage);
}

ftn_error_t ftn_storage_initialize(ftn_storage_t* storage) {
    if (!storage) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Create base directories if they don't exist */
    if (storage->news_root) {
        if (ftn_storage_ensure_directory(storage->news_root, FTN_STORAGE_DIR_MODE) != FTN_OK) {
            return FTN_ERROR_FILE;
        }
    }

    if (storage->mail_root) {
        if (ftn_storage_ensure_directory(storage->mail_root, FTN_STORAGE_DIR_MODE) != FTN_OK) {
            return FTN_ERROR_FILE;
        }
    }

    return FTN_OK;
}

/* Maildir operations */
ftn_error_t ftn_storage_create_maildir(const char* path) {
    char* tmp_path;
    char* new_path;
    char* cur_path;
    ftn_error_t result = FTN_OK;

    if (!path) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Create main directory */
    if (ftn_storage_ensure_directory(path, FTN_STORAGE_DIR_MODE) != FTN_OK) {
        return FTN_ERROR_FILE;
    }

    /* Create subdirectories */
    tmp_path = malloc(strlen(path) + strlen(FTN_MAILDIR_TMP) + 2);
    new_path = malloc(strlen(path) + strlen(FTN_MAILDIR_NEW) + 2);
    cur_path = malloc(strlen(path) + strlen(FTN_MAILDIR_CUR) + 2);

    if (!tmp_path || !new_path || !cur_path) {
        result = FTN_ERROR_NOMEM;
        goto cleanup;
    }

    sprintf(tmp_path, "%s/%s", path, FTN_MAILDIR_TMP);
    sprintf(new_path, "%s/%s", path, FTN_MAILDIR_NEW);
    sprintf(cur_path, "%s/%s", path, FTN_MAILDIR_CUR);

    if (ftn_storage_ensure_directory(tmp_path, FTN_STORAGE_DIR_MODE) != FTN_OK ||
        ftn_storage_ensure_directory(new_path, FTN_STORAGE_DIR_MODE) != FTN_OK ||
        ftn_storage_ensure_directory(cur_path, FTN_STORAGE_DIR_MODE) != FTN_OK) {
        result = FTN_ERROR_FILE;
    }

cleanup:
    ftn_storage_safe_free(tmp_path);
    ftn_storage_safe_free(new_path);
    ftn_storage_safe_free(cur_path);

    return result;
}

ftn_error_t ftn_storage_generate_maildir_filename(ftn_maildir_file_t* file_info, const char* maildir_path) {
    char hostname[256];
    time_t now;
    pid_t pid;

    if (!file_info || !maildir_path) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    memset(file_info, 0, sizeof(ftn_maildir_file_t));

    /* Get current time and process ID */
    time(&now);
    pid = getpid();

    /* Get hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "localhost");
    }
    hostname[sizeof(hostname) - 1] = '\0';

    /* Generate filename: timestamp.pid.hostname */
    file_info->filename = malloc(64);
    if (!file_info->filename) {
        return FTN_ERROR_NOMEM;
    }
    sprintf(file_info->filename, "%ld.%d.%s", (long)now, (int)pid, hostname);

    /* Generate full paths */
    file_info->tmp_path = malloc(strlen(maildir_path) + strlen(file_info->filename) + 6);
    file_info->new_path = malloc(strlen(maildir_path) + strlen(file_info->filename) + 6);

    if (!file_info->tmp_path || !file_info->new_path) {
        ftn_maildir_file_free(file_info);
        return FTN_ERROR_NOMEM;
    }

    sprintf(file_info->tmp_path, "%s/%s/%s", maildir_path, FTN_MAILDIR_TMP, file_info->filename);
    sprintf(file_info->new_path, "%s/%s/%s", maildir_path, FTN_MAILDIR_NEW, file_info->filename);

    return FTN_OK;
}

void ftn_maildir_file_free(ftn_maildir_file_t* file_info) {
    if (!file_info) return;

    ftn_storage_safe_free(file_info->filename);
    ftn_storage_safe_free(file_info->tmp_path);
    ftn_storage_safe_free(file_info->new_path);
    memset(file_info, 0, sizeof(ftn_maildir_file_t));
}

ftn_error_t ftn_storage_store_mail(ftn_storage_t* storage, const ftn_message_t* msg,
                                  const char* username, const char* network) {
    char* expanded_path = NULL;
    char* rfc822_text = NULL;
    char* domain = NULL;
    ftn_maildir_file_t file_info;
    FILE* tmp_file = NULL;
    ftn_error_t result = FTN_OK;
    const ftn_network_config_t* net_config;

    if (!storage || !msg || !username || !network) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    if (!storage->mail_root) {
        return FTN_ERROR_INVALID;
    }

    /* Get network configuration for domain */
    net_config = ftn_config_get_network(storage->config, network);
    if (net_config && net_config->domain) {
        domain = net_config->domain;
    } else {
        domain = "fidonet.org";  /* Default domain */
    }

    /* Expand path template */
    expanded_path = ftn_storage_expand_path(storage->mail_root, username, network);
    if (!expanded_path) {
        return FTN_ERROR_NOMEM;
    }

    /* Create maildir if it doesn't exist */
    result = ftn_storage_create_maildir(expanded_path);
    if (result != FTN_OK) {
        goto cleanup;
    }

    /* Convert FTN message to RFC822 */
    result = ftn_storage_convert_to_rfc822(msg, domain, &rfc822_text);
    if (result != FTN_OK) {
        goto cleanup;
    }

    /* Generate maildir filename */
    result = ftn_storage_generate_maildir_filename(&file_info, expanded_path);
    if (result != FTN_OK) {
        goto cleanup;
    }

    /* Write to tmp directory first (atomic operation) */
    tmp_file = fopen(file_info.tmp_path, "w");
    if (!tmp_file) {
        result = FTN_ERROR_FILE;
        goto cleanup;
    }

    if (fputs(rfc822_text, tmp_file) == EOF) {
        result = FTN_ERROR_FILE;
        goto cleanup;
    }

    fclose(tmp_file);
    tmp_file = NULL;

    /* Move to new directory */
    if (rename(file_info.tmp_path, file_info.new_path) != 0) {
        result = FTN_ERROR_FILE;
        unlink(file_info.tmp_path);  /* Clean up temp file */
        goto cleanup;
    }

cleanup:
    if (tmp_file) {
        fclose(tmp_file);
        unlink(file_info.tmp_path);  /* Clean up on error */
    }

    ftn_maildir_file_free(&file_info);
    ftn_storage_safe_free(expanded_path);
    ftn_storage_safe_free(rfc822_text);

    return result;
}

/* USENET spool operations */
ftn_error_t ftn_storage_store_news(ftn_storage_t* storage, const ftn_message_t* msg,
                                  const char* area, const char* network) {
    char* newsgroup = NULL;
    char* usenet_text = NULL;
    char* article_dir = NULL;
    char* article_path = NULL;
    long article_num = 0;
    ftn_error_t result = FTN_OK;

    if (!storage || !msg || !area || !network) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    if (!storage->news_root) {
        return FTN_ERROR_INVALID;
    }

    /* Generate newsgroup name */
    newsgroup = ftn_area_to_newsgroup(network, area);
    if (!newsgroup) {
        return FTN_ERROR_NOMEM;
    }

    /* Convert FTN message to USENET format */
    result = ftn_storage_convert_to_usenet(msg, network, &usenet_text);
    if (result != FTN_OK) {
        goto cleanup;
    }

    /* Create newsgroup directory if needed */
    result = ftn_storage_create_newsgroup(storage, newsgroup);
    if (result != FTN_OK) {
        goto cleanup;
    }

    /* Get next article number */
    result = ftn_storage_get_next_article_number(storage, newsgroup, &article_num);
    if (result != FTN_OK) {
        goto cleanup;
    }

    /* Build article directory path */
    article_dir = malloc(strlen(storage->news_root) + strlen(newsgroup) + 2);
    if (!article_dir) {
        result = FTN_ERROR_NOMEM;
        goto cleanup;
    }
    sprintf(article_dir, "%s/%s", storage->news_root, newsgroup);

    /* Replace dots with slashes in newsgroup name for directory structure */
    {
        char* p = article_dir + strlen(storage->news_root) + 1;
        while (*p) {
            if (*p == '.') *p = '/';
            p++;
        }
    }

    /* Build article file path */
    article_path = malloc(strlen(article_dir) + 32);
    if (!article_path) {
        result = FTN_ERROR_NOMEM;
        goto cleanup;
    }
    sprintf(article_path, "%s/%ld", article_dir, article_num);

    /* Write article file atomically */
    result = ftn_storage_write_file_atomic(article_path, usenet_text, strlen(usenet_text));
    if (result != FTN_OK) {
        goto cleanup;
    }

    /* Update active file */
    result = ftn_storage_update_active_file(storage, newsgroup, article_num);

cleanup:
    ftn_storage_safe_free(newsgroup);
    ftn_storage_safe_free(usenet_text);
    ftn_storage_safe_free(article_dir);
    ftn_storage_safe_free(article_path);

    return result;
}

ftn_error_t ftn_storage_create_newsgroup(ftn_storage_t* storage, const char* newsgroup) {
    char* dir_path;
    char* work_path;
    char* p;
    ftn_error_t result = FTN_OK;

    if (!storage || !newsgroup || !storage->news_root) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Build directory path */
    dir_path = malloc(strlen(storage->news_root) + strlen(newsgroup) + 2);
    if (!dir_path) {
        return FTN_ERROR_NOMEM;
    }
    sprintf(dir_path, "%s/%s", storage->news_root, newsgroup);

    /* Replace dots with slashes */
    work_path = ftn_storage_strdup(dir_path);
    if (!work_path) {
        free(dir_path);
        return FTN_ERROR_NOMEM;
    }

    p = work_path + strlen(storage->news_root) + 1;
    while (*p) {
        if (*p == '.') *p = '/';
        p++;
    }

    /* Create directory recursively */
    result = ftn_storage_create_directory_recursive(work_path, FTN_STORAGE_DIR_MODE);

    free(dir_path);
    free(work_path);

    return result;
}

/* Message list utilities */
ftn_message_list_t* ftn_message_list_new(void) {
    ftn_message_list_t* list = malloc(sizeof(ftn_message_list_t));
    if (list) {
        list->messages = NULL;
        list->count = 0;
        list->capacity = 0;
    }
    return list;
}

void ftn_message_list_free(ftn_message_list_t* list) {
    if (!list) return;

    if (list->messages) {
        size_t i;
        for (i = 0; i < list->count; i++) {
            if (list->messages[i]) {
                ftn_message_free(list->messages[i]);
            }
        }
        free(list->messages);
    }

    free(list);
}

/* Utility functions */
char* ftn_storage_expand_path(const char* template, const char* username, const char* network) {
    char* result;
    char* src;
    char* dst;
    size_t max_len;
    const char* user_str = username ? username : "";
    const char* net_str = network ? network : "";

    if (!template) {
        return NULL;
    }

    /* Calculate maximum possible length (very conservative) */
    max_len = strlen(template) +
              (username ? strlen(username) * 10 : 0) +
              (network ? strlen(network) * 10 : 0) + 100;

    result = malloc(max_len);
    if (!result) {
        return NULL;
    }

    /* Simple character-by-character substitution */
    src = (char*)template;
    dst = result;

    while (*src && (dst - result) < (int)max_len - 50) {
        if (strncmp(src, "%USER%", 6) == 0) {
            strcpy(dst, user_str);
            dst += strlen(user_str);
            src += 6;
        } else if (strncmp(src, "%NETWORK%", 9) == 0) {
            strcpy(dst, net_str);
            dst += strlen(net_str);
            src += 9;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    /* Reallocate to actual size */
    {
        char* trimmed = malloc(strlen(result) + 1);
        if (trimmed) {
            strcpy(trimmed, result);
            free(result);
            result = trimmed;
        }
    }

    return result;
}

ftn_error_t ftn_storage_ensure_directory(const char* path, mode_t mode) {
    struct stat st;

    if (!path) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    if (stat(path, &st) == 0) {
        /* Directory exists, check if it's actually a directory */
        if (S_ISDIR(st.st_mode)) {
            return FTN_OK;
        } else {
            return FTN_ERROR_INVALID;  /* Path exists but is not a directory */
        }
    }

    /* Directory doesn't exist, create it */
    if (mkdir(path, mode) == 0) {
        return FTN_OK;
    }

    return FTN_ERROR_FILE;
}

ftn_error_t ftn_storage_create_directory_recursive(const char* path, mode_t mode) {
    char* path_copy;
    char* p;
    ftn_error_t result = FTN_OK;

    if (!path) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    path_copy = ftn_storage_strdup(path);
    if (!path_copy) {
        return FTN_ERROR_NOMEM;
    }

    /* Create directories from root to target */
    for (p = path_copy + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (ftn_storage_ensure_directory(path_copy, mode) != FTN_OK) {
                result = FTN_ERROR_FILE;
                goto cleanup;
            }
            *p = '/';
        }
    }

    /* Create the final directory */
    if (ftn_storage_ensure_directory(path_copy, mode) != FTN_OK) {
        result = FTN_ERROR_FILE;
    }

cleanup:
    free(path_copy);
    return result;
}

/* Conversion integration functions */
ftn_error_t ftn_storage_convert_to_rfc822(const ftn_message_t* ftn_msg, const char* domain, char** rfc822_text) {
    rfc822_message_t* rfc_msg = NULL;
    ftn_error_t result;

    if (!ftn_msg || !domain || !rfc822_text) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Convert FTN to RFC822 */
    result = ftn_to_rfc822(ftn_msg, domain, &rfc_msg);
    if (result != FTN_OK) {
        return result;
    }

    /* Generate text representation */
    *rfc822_text = rfc822_message_to_text(rfc_msg);
    if (!*rfc822_text) {
        result = FTN_ERROR_NOMEM;
    }

    rfc822_message_free(rfc_msg);
    return result;
}

ftn_error_t ftn_storage_convert_to_usenet(const ftn_message_t* ftn_msg, const char* network, char** usenet_text) {
    rfc822_message_t* usenet_msg = NULL;
    ftn_error_t result;

    if (!ftn_msg || !network || !usenet_text) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Convert FTN to USENET */
    result = ftn_to_usenet(ftn_msg, network, &usenet_msg);
    if (result != FTN_OK) {
        return result;
    }

    /* Generate text representation */
    *usenet_text = rfc822_message_to_text(usenet_msg);
    if (!*usenet_text) {
        result = FTN_ERROR_NOMEM;
    }

    rfc822_message_free(usenet_msg);
    return result;
}

/* Placeholder implementations for remaining functions */
ftn_error_t ftn_storage_update_active_file(ftn_storage_t* storage, const char* newsgroup, long article_num) {
    /* TODO: Implement active file management */
    (void)storage; (void)newsgroup; (void)article_num;
    return FTN_OK;
}

ftn_error_t ftn_storage_get_next_article_number(ftn_storage_t* storage, const char* newsgroup, long* article_num) {
    /* TODO: Implement article number tracking */
    (void)storage; (void)newsgroup;
    *article_num = 1;  /* Default to article 1 for now */
    return FTN_OK;
}

ftn_error_t ftn_storage_write_file_atomic(const char* path, const char* content, size_t length) {
    char* temp_path;
    FILE* file;
    size_t written;

    if (!path || !content) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Create temporary file name */
    temp_path = malloc(strlen(path) + 5);
    if (!temp_path) {
        return FTN_ERROR_NOMEM;
    }
    sprintf(temp_path, "%s.tmp", path);

    /* Write to temporary file */
    file = fopen(temp_path, "w");
    if (!file) {
        free(temp_path);
        return FTN_ERROR_FILE;
    }

    written = fwrite(content, 1, length, file);
    fclose(file);

    if (written != length) {
        unlink(temp_path);
        free(temp_path);
        return FTN_ERROR_FILE;
    }

    /* Atomically rename to final name */
    if (rename(temp_path, path) != 0) {
        unlink(temp_path);
        free(temp_path);
        return FTN_ERROR_FILE;
    }

    free(temp_path);
    return FTN_OK;
}

ftn_error_t ftn_message_list_add(ftn_message_list_t* list, ftn_message_t* message) {
    if (!list || !message) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Expand capacity if needed */
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity ? list->capacity * 2 : 8;
        ftn_message_t** new_messages = realloc(list->messages,
                                              new_capacity * sizeof(ftn_message_t*));
        if (!new_messages) {
            return FTN_ERROR_NOMEM;
        }
        list->messages = new_messages;
        list->capacity = new_capacity;
    }

    /* Add message to list */
    list->messages[list->count] = message;
    list->count++;

    return FTN_OK;
}

ftn_error_t ftn_message_list_clear(ftn_message_list_t* list) {
    size_t i;

    if (!list) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Free all messages */
    for (i = 0; i < list->count; i++) {
        if (list->messages[i]) {
            ftn_message_free(list->messages[i]);
        }
    }

    /* Reset list */
    list->count = 0;

    return FTN_OK;
}

/* Additional placeholder implementations */
ftn_error_t ftn_storage_scan_outbound_mail(ftn_storage_t* storage, const char* username,
                                          const char* network, ftn_message_list_t* messages) {
    (void)storage; (void)username; (void)network; (void)messages;
    return FTN_OK;  /* TODO: Implement */
}

ftn_error_t ftn_storage_scan_outbound_news(ftn_storage_t* storage, const char* area,
                                          const char* network, ftn_message_list_t* messages) {
    (void)storage; (void)area; (void)network; (void)messages;
    return FTN_OK;  /* TODO: Implement */
}