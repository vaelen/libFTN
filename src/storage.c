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

#define _POSIX_C_SOURCE 200112L
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
#include <dirent.h>

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
    char* lowercase_area = NULL;
    long article_num = 0;
    ftn_error_t result = FTN_OK;

    if (!storage || !msg || !area || !network) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    if (!storage->news_root) {
        return FTN_ERROR_INVALID;
    }

    /* Convert area to lowercase for directory structure */
    lowercase_area = ftn_storage_sanitize_area_name(area);
    if (!lowercase_area) {
        return FTN_ERROR_NOMEM;
    }

    /* Generate newsgroup name */
    newsgroup = ftn_area_to_newsgroup(network, area);
    if (!newsgroup) {
        free(lowercase_area);
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

    /* Build article directory path using lowercase area name */
    article_dir = malloc(strlen(storage->news_root) + strlen(network) + strlen(lowercase_area) + 4);
    if (!article_dir) {
        result = FTN_ERROR_NOMEM;
        goto cleanup;
    }
    sprintf(article_dir, "%s/%s/%s", storage->news_root, network, lowercase_area);

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
    ftn_storage_safe_free(lowercase_area);

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

/* Advanced filename generation (from pkt2mail.c) */
ftn_error_t ftn_storage_generate_message_filename(const ftn_message_t* msg, char** filename) {
    char* result = NULL;
    char timestamp_str[32];
    char from_addr[64];
    char to_addr[64];
    struct tm* tm_info;

    if (!msg || !filename) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    *filename = NULL;

    /* Use MSGID if available */
    if (msg->msgid && *msg->msgid) {
        result = malloc(strlen(msg->msgid) + 1);
        if (!result) {
            return FTN_ERROR_NOMEM;
        }

        strcpy(result, msg->msgid);

        /* Remove angle brackets if present */
        if (result[0] == '<' && result[strlen(result)-1] == '>') {
            memmove(result, result + 1, strlen(result) - 2);
            result[strlen(result) - 2] = '\0';
        }

        /* Sanitize filename in place */
        {
            char* p;
            for (p = result; *p; p++) {
                if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' ||
                    *p == '?' || *p == '"' || *p == '<' || *p == '>' ||
                    *p == '|' || *p == ' ' || *p == '\t' || *p == '\n' ||
                    *p == '\r') {
                    *p = '_';
                }
            }
        }
        *filename = result;
        return FTN_OK;
    }

    /* Generate FROM_TO_DATE filename */
    ftn_address_to_string(&msg->orig_addr, from_addr, sizeof(from_addr));
    ftn_address_to_string(&msg->dest_addr, to_addr, sizeof(to_addr));

    if (msg->timestamp > 0) {
        tm_info = gmtime(&msg->timestamp);
        if (tm_info) {
            snprintf(timestamp_str, sizeof(timestamp_str), "%04d%02d%02d_%02d%02d%02d",
                     tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                     tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
        } else {
            strcpy(timestamp_str, "unknown");
        }
    } else {
        /* Use current time if no timestamp available */
        time_t now = time(NULL);
        tm_info = gmtime(&now);
        if (tm_info) {
            snprintf(timestamp_str, sizeof(timestamp_str), "%04d%02d%02d_%02d%02d%02d",
                     tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                     tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
        } else {
            strcpy(timestamp_str, "unknown");
        }
    }

    /* Allocate filename buffer */
    result = malloc(strlen(from_addr) + strlen(to_addr) + strlen(timestamp_str) + 8);
    if (!result) {
        return FTN_ERROR_NOMEM;
    }

    snprintf(result, strlen(from_addr) + strlen(to_addr) + strlen(timestamp_str) + 8,
             "%s_%s_%s", from_addr, to_addr, timestamp_str);

    /* Sanitize filename in place */
    {
        char* p;
        for (p = result; *p; p++) {
            if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' ||
                *p == '?' || *p == '"' || *p == '<' || *p == '>' ||
                *p == '|' || *p == ' ' || *p == '\t' || *p == '\n' ||
                *p == '\r') {
                *p = '_';
            }
        }
    }

    *filename = result;
    return FTN_OK;
}

/* Message duplicate detection (from pkt2mail.c) */
ftn_error_t ftn_storage_message_exists(ftn_storage_t* storage, const char* maildir_path,
                                      const ftn_message_t* msg, const char* filename, int* exists) {
    char* actual_maildir_path = NULL;
    char path[512];
    struct stat st;
    DIR* dir;
    struct dirent* entry;
    int result_exists = 0;
    ftn_error_t result = FTN_OK;

    if (!storage || !maildir_path || !msg || !filename || !exists) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    *exists = 0;

    /* Expand user path if needed */
    if (strstr(maildir_path, "%USER%")) {
        actual_maildir_path = ftn_storage_expand_path(maildir_path, msg->to_user, NULL);
        if (!actual_maildir_path) {
            return FTN_ERROR_NOMEM;
        }
    } else {
        actual_maildir_path = ftn_storage_strdup(maildir_path);
        if (!actual_maildir_path) {
            return FTN_ERROR_NOMEM;
        }
    }

    /* Check in new directory */
    snprintf(path, sizeof(path), "%s/new/%s", actual_maildir_path, filename);
    if (stat(path, &st) == 0) {
        result_exists = 1;
        goto cleanup;
    }

    /* Check in cur directory */
    snprintf(path, sizeof(path), "%s/cur/%s", actual_maildir_path, filename);
    if (stat(path, &st) == 0) {
        result_exists = 1;
        goto cleanup;
    }

    /* Also check for files with additional maildir flags */
    snprintf(path, sizeof(path), "%s/cur", actual_maildir_path);
    dir = opendir(path);
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, filename, strlen(filename)) == 0) {
                /* Found a match (possibly with maildir flags) */
                closedir(dir);
                result_exists = 1;
                goto cleanup;
            }
        }
        closedir(dir);
    }

cleanup:
    free(actual_maildir_path);
    *exists = result_exists;
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

/* Active file and article number management */
ftn_error_t ftn_storage_get_next_article_number(ftn_storage_t* storage, const char* newsgroup, long* article_num) {
    char area_path[512];
    char* newsgroup_copy;
    char* network_part;
    char* area_part;
    char* p;
    DIR* dir;
    struct dirent* entry;
    long max_num = 0;
    long num;

    if (!storage || !newsgroup || !article_num || !storage->news_root) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Parse newsgroup name to extract network and area (format: network.area) */
    newsgroup_copy = ftn_storage_strdup(newsgroup);
    if (!newsgroup_copy) {
        return FTN_ERROR_NOMEM;
    }

    network_part = newsgroup_copy;
    area_part = strchr(newsgroup_copy, '.');
    if (area_part) {
        *area_part = '\0';
        area_part++;
    } else {
        /* No dot found, assume entire string is area with default network */
        area_part = newsgroup_copy;
        network_part = "fidonet";
    }

    /* Construct area directory path: news_root/network/area */
    snprintf(area_path, sizeof(area_path), "%s/%s/%s", storage->news_root, network_part, area_part);

    /* Convert dots in area name to slashes for filesystem */
    for (p = strstr(area_path, area_part); p && *p; p++) {
        if (*p == '.') *p = '/';
    }

    dir = opendir(area_path);
    if (!dir) {
        /* Directory doesn't exist, start with 1 */
        free(newsgroup_copy);
        *article_num = 1;
        return FTN_OK;
    }

    /* Find highest existing article number */
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.' && sscanf(entry->d_name, "%ld", &num) == 1) {
            if (num > max_num) {
                max_num = num;
            }
        }
    }

    closedir(dir);
    free(newsgroup_copy);
    *article_num = max_num + 1;
    return FTN_OK;
}

ftn_error_t ftn_storage_update_active_file(ftn_storage_t* storage, const char* newsgroup, long article_num) {
    char temp_path[512];
    FILE* active_fp = NULL;
    FILE* temp_fp = NULL;
    char line[1024];
    char existing_newsgroup[256];
    long existing_high, existing_low;
    char existing_perm;
    int found = 0;
    ftn_error_t result = FTN_OK;

    if (!storage || !newsgroup || !storage->active_file_path) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    snprintf(temp_path, sizeof(temp_path), "%s.tmp", storage->active_file_path);

    /* Open active file for reading (may not exist) */
    active_fp = fopen(storage->active_file_path, "r");
    temp_fp = fopen(temp_path, "w");
    if (!temp_fp) {
        if (active_fp) fclose(active_fp);
        return FTN_ERROR_FILE;
    }

    /* Copy existing entries, updating the newsgroup if found */
    if (active_fp) {
        while (fgets(line, sizeof(line), active_fp)) {
            if (sscanf(line, "%255s %ld %ld %c", existing_newsgroup, &existing_high, &existing_low, &existing_perm) == 4) {
                if (strcmp(existing_newsgroup, newsgroup) == 0) {
                    /* Update this newsgroup */
                    if (article_num > existing_high) {
                        existing_high = article_num;
                    }
                    if (existing_low == 0 || article_num < existing_low) {
                        existing_low = article_num;
                    }
                    fprintf(temp_fp, "%s %ld %ld %c\n", newsgroup, existing_high, existing_low, existing_perm);
                    found = 1;
                } else {
                    /* Copy existing entry */
                    fputs(line, temp_fp);
                }
            } else {
                /* Copy malformed line as-is */
                fputs(line, temp_fp);
            }
        }
        fclose(active_fp);
    }

    /* Add new newsgroup if not found */
    if (!found) {
        fprintf(temp_fp, "%s %ld %ld y\n", newsgroup, article_num, article_num);
    }

    fclose(temp_fp);

    /* Replace active file with temp file */
    if (rename(temp_path, storage->active_file_path) != 0) {
        unlink(temp_path);
        result = FTN_ERROR_FILE;
    }

    return result;
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

/* Path sanitization functions */
char* ftn_storage_sanitize_filename(const char* input) {
    char* result;
    char* p;

    if (!input) {
        return NULL;
    }

    result = ftn_storage_strdup(input);
    if (!result) {
        return NULL;
    }

    /* Replace special characters with underscores */
    for (p = result; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' ||
            *p == '?' || *p == '"' || *p == '<' || *p == '>' ||
            *p == '|' || *p == ' ' || *p == '\t' || *p == '\n' ||
            *p == '\r') {
            *p = '_';
        }
    }

    return result;
}

char* ftn_storage_sanitize_username(const char* username) {
    char* result;
    char* p;

    if (!username) {
        return NULL;
    }

    result = ftn_storage_strdup(username);
    if (!result) {
        return NULL;
    }

    /* Convert to lowercase and sanitize */
    for (p = result; *p; p++) {
        *p = tolower((unsigned char)*p);
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' ||
            *p == '?' || *p == '"' || *p == '<' || *p == '>' ||
            *p == '|' || *p == ' ' || *p == '\t' || *p == '\n' ||
            *p == '\r') {
            *p = '_';
        }
    }

    return result;
}

char* ftn_storage_sanitize_area_name(const char* area) {
    char* result;
    char* p;

    if (!area) {
        return NULL;
    }

    result = ftn_storage_strdup(area);
    if (!result) {
        return NULL;
    }

    /* Convert to lowercase */
    for (p = result; *p; p++) {
        *p = tolower((unsigned char)*p);
    }

    return result;
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

/* Utility-compatible storage functions */
ftn_error_t ftn_storage_store_mail_with_options(ftn_storage_t* storage, const ftn_message_t* msg,
                                               const char* maildir_path, const char* domain,
                                               const char* user_filter) {
    ftn_maildir_file_t file_info;
    char* rfc822_text;
    FILE* file;
    ftn_error_t error;
    int exists;
    char* filename;

    (void)user_filter; /* Currently unused, reserved for future filtering */

    if (!msg || !maildir_path || !domain) {
        return FTN_ERROR_INVALID;
    }

    /* Create maildir structure if it doesn't exist */
    error = ftn_storage_create_maildir(maildir_path);
    if (error != FTN_OK) {
        return error;
    }

    /* Generate filename based on message */
    error = ftn_storage_generate_message_filename(msg, &filename);
    if (error != FTN_OK) {
        return error;
    }

    /* Check for duplicates */
    error = ftn_storage_message_exists(storage, maildir_path, msg, filename, &exists);
    if (error != FTN_OK) {
        free(filename);
        return error;
    }

    if (exists) {
        free(filename);
        return FTN_OK; /* Already exists, skip silently */
    }

    /* Generate maildir filename structure */
    memset(&file_info, 0, sizeof(file_info));
    file_info.filename = filename;

    /* Build paths */
    file_info.tmp_path = malloc(strlen(maildir_path) + strlen(FTN_MAILDIR_TMP) + strlen(filename) + 3);
    file_info.new_path = malloc(strlen(maildir_path) + strlen(FTN_MAILDIR_NEW) + strlen(filename) + 3);

    if (!file_info.tmp_path || !file_info.new_path) {
        ftn_maildir_file_free(&file_info);
        return FTN_ERROR_NOMEM;
    }

    sprintf(file_info.tmp_path, "%s/%s/%s", maildir_path, FTN_MAILDIR_TMP, filename);
    sprintf(file_info.new_path, "%s/%s/%s", maildir_path, FTN_MAILDIR_NEW, filename);

    /* Convert to RFC822 format */
    error = ftn_storage_convert_to_rfc822(msg, domain, &rfc822_text);
    if (error != FTN_OK) {
        ftn_maildir_file_free(&file_info);
        return error;
    }

    /* Write to tmp directory first */
    file = fopen(file_info.tmp_path, "w");
    if (!file) {
        free(rfc822_text);
        ftn_maildir_file_free(&file_info);
        return FTN_ERROR_FILE;
    }

    if (fputs(rfc822_text, file) == EOF) {
        fclose(file);
        free(rfc822_text);
        ftn_maildir_file_free(&file_info);
        unlink(file_info.tmp_path);
        return FTN_ERROR_FILE;
    }

    fclose(file);
    free(rfc822_text);

    /* Atomically move to new directory */
    if (rename(file_info.tmp_path, file_info.new_path) != 0) {
        unlink(file_info.tmp_path);
        ftn_maildir_file_free(&file_info);
        return FTN_ERROR_FILE;
    }

    ftn_maildir_file_free(&file_info);
    return FTN_OK;
}

ftn_error_t ftn_storage_store_news_with_options(ftn_storage_t* storage, const ftn_message_t* msg,
                                               const char* usenet_root, const char* network) {
    char* area_path;
    char* sanitized_area;
    char* article_path;
    char* usenet_text;
    long article_num;
    ftn_error_t error;
    const char* area;

    if (!msg || !usenet_root || !network) {
        return FTN_ERROR_INVALID;
    }

    /* Get area from message */
    area = msg->area;
    if (!area || !*area) {
        return FTN_ERROR_INVALID;
    }

    /* Sanitize area name */
    sanitized_area = ftn_storage_sanitize_area_name(area);
    if (!sanitized_area) {
        return FTN_ERROR_NOMEM;
    }

    /* Build area directory path */
    area_path = malloc(strlen(usenet_root) + strlen(network) + strlen(sanitized_area) + 3);
    if (!area_path) {
        free(sanitized_area);
        return FTN_ERROR_NOMEM;
    }
    sprintf(area_path, "%s/%s/%s", usenet_root, network, sanitized_area);

    /* Create directory structure */
    error = ftn_storage_create_directory_recursive(area_path, FTN_STORAGE_DIR_MODE);
    if (error != FTN_OK) {
        free(sanitized_area);
        free(area_path);
        return error;
    }

    /* Get next article number */
    error = ftn_storage_get_next_article_number(storage, area, &article_num);
    if (error != FTN_OK) {
        free(sanitized_area);
        free(area_path);
        return error;
    }

    /* Build article file path */
    article_path = malloc(strlen(area_path) + 32);
    if (!article_path) {
        free(sanitized_area);
        free(area_path);
        return FTN_ERROR_NOMEM;
    }
    sprintf(article_path, "%s/%ld", area_path, article_num);

    /* Convert to USENET format */
    error = ftn_storage_convert_to_usenet(msg, network, &usenet_text);
    if (error != FTN_OK) {
        free(sanitized_area);
        free(area_path);
        free(article_path);
        return error;
    }

    /* Write article file */
    error = ftn_storage_write_file_atomic(article_path, usenet_text, strlen(usenet_text));
    if (error != FTN_OK) {
        free(sanitized_area);
        free(area_path);
        free(article_path);
        free(usenet_text);
        return error;
    }

    /* Update active file */
    error = ftn_storage_update_active_file(storage, area, article_num);
    if (error != FTN_OK) {
        /* Article was written but active file update failed - not critical */
    }

    free(sanitized_area);
    free(area_path);
    free(article_path);
    free(usenet_text);
    return FTN_OK;
}

/* Simple storage functions that don't require full storage context */
ftn_error_t ftn_storage_store_mail_simple(const ftn_message_t* msg, const char* maildir_path,
                                         const char* domain, const char* user_filter) {
    return ftn_storage_store_mail_with_options(NULL, msg, maildir_path, domain, user_filter);
}

ftn_error_t ftn_storage_store_news_simple(const ftn_message_t* msg, const char* usenet_root,
                                         const char* network) {
    return ftn_storage_store_news_with_options(NULL, msg, usenet_root, network);
}

/* Lock file operations for atomic updates */
ftn_error_t ftn_storage_acquire_lock(const char* lock_path, int* lock_fd) {
    int fd;
    int retry_count = 0;
    const int max_retries = 50;

    if (!lock_path || !lock_fd) {
        return FTN_ERROR_INVALID;
    }

    while (retry_count < max_retries) {
        fd = open(lock_path, O_CREAT | O_EXCL | O_WRONLY, FTN_STORAGE_FILE_MODE);
        if (fd >= 0) {
            *lock_fd = fd;
            return FTN_OK;
        }

        if (errno != EEXIST) {
            return FTN_ERROR_FILE;
        }

        /* Lock file exists, wait a bit and retry */
        sleep(1); /* 1 second - use sleep instead of usleep for C89 compatibility */
        retry_count++;
    }

    return FTN_ERROR_FILE; /* Timeout acquiring lock */
}

ftn_error_t ftn_storage_release_lock(int lock_fd, const char* lock_path) {
    if (lock_fd < 0 || !lock_path) {
        return FTN_ERROR_INVALID;
    }

    close(lock_fd);
    unlink(lock_path);
    return FTN_OK;
}