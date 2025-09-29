/*
 * bso.c - BinkleyTerm Style Outbound directory management
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
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include "ftn/bso.h"
#include "ftn/log.h"

/* Need to include address structure */
typedef struct ftn_address {
    int zone;
    int net;
    int node;
    int point;
    char* domain;
} ftn_address_t;

ftn_bso_error_t ftn_bso_path_init(ftn_bso_path_t* bso_path) {
    if (!bso_path) {
        return BSO_ERROR_INVALID_PATH;
    }

    memset(bso_path, 0, sizeof(ftn_bso_path_t));
    return BSO_OK;
}

void ftn_bso_path_free(ftn_bso_path_t* bso_path) {
    if (!bso_path) {
        return;
    }

    if (bso_path->base_path) {
        free(bso_path->base_path);
        bso_path->base_path = NULL;
    }

    if (bso_path->domain) {
        free(bso_path->domain);
        bso_path->domain = NULL;
    }

    if (bso_path->address) {
        if (bso_path->address->domain) {
            free(bso_path->address->domain);
        }
        free(bso_path->address);
        bso_path->address = NULL;
    }

    memset(bso_path, 0, sizeof(ftn_bso_path_t));
}

ftn_bso_error_t ftn_bso_set_base_path(ftn_bso_path_t* bso_path, const char* base_path) {
    if (!bso_path || !base_path) {
        return BSO_ERROR_INVALID_PATH;
    }

    if (bso_path->base_path) {
        free(bso_path->base_path);
    }

    bso_path->base_path = malloc(strlen(base_path) + 1);
    if (!bso_path->base_path) {
        return BSO_ERROR_MEMORY;
    }

    strcpy(bso_path->base_path, base_path);
    return BSO_OK;
}

ftn_bso_error_t ftn_bso_set_domain(ftn_bso_path_t* bso_path, const char* domain) {
    if (!bso_path) {
        return BSO_ERROR_INVALID_PATH;
    }

    if (bso_path->domain) {
        free(bso_path->domain);
        bso_path->domain = NULL;
    }

    if (domain) {
        bso_path->domain = malloc(strlen(domain) + 1);
        if (!bso_path->domain) {
            return BSO_ERROR_MEMORY;
        }
        strcpy(bso_path->domain, domain);
    }

    return BSO_OK;
}

ftn_bso_error_t ftn_bso_set_zone(ftn_bso_path_t* bso_path, int zone) {
    if (!bso_path || zone < 1) {
        return BSO_ERROR_INVALID_PATH;
    }

    bso_path->zone = zone;
    return BSO_OK;
}

ftn_bso_error_t ftn_bso_set_address(ftn_bso_path_t* bso_path, const struct ftn_address* address) {
    if (!bso_path || !address) {
        return BSO_ERROR_INVALID_ADDRESS;
    }

    if (bso_path->address) {
        if (bso_path->address->domain) {
            free(bso_path->address->domain);
        }
        free(bso_path->address);
    }

    bso_path->address = malloc(sizeof(ftn_address_t));
    if (!bso_path->address) {
        return BSO_ERROR_MEMORY;
    }

    memcpy(bso_path->address, address, sizeof(ftn_address_t));

    /* Copy domain string if present */
    bso_path->address->domain = NULL;
    if (address->domain) {
        bso_path->address->domain = malloc(strlen(address->domain) + 1);
        if (bso_path->address->domain) {
            strcpy(bso_path->address->domain, address->domain);
        }
    }

    return BSO_OK;
}

char* ftn_bso_get_outbound_path(const ftn_bso_path_t* bso_path) {
    char* result;
    size_t len;

    if (!bso_path || !bso_path->base_path) {
        return NULL;
    }

    /* For zone 1, use base path directly. For other zones, append .00X */
    if (bso_path->zone == 1) {
        len = strlen(bso_path->base_path) + 1;
        result = malloc(len);
        if (result) {
            strcpy(result, bso_path->base_path);
        }
    } else {
        len = strlen(bso_path->base_path) + 10;
        result = malloc(len);
        if (result) {
            snprintf(result, len, "%s.%03x", bso_path->base_path, bso_path->zone);
        }
    }

    return result;
}

char* ftn_bso_get_zone_path(const char* base_path, int zone) {
    char* result;
    size_t len;

    if (!base_path || zone < 1) {
        return NULL;
    }

    if (zone == 1) {
        len = strlen(base_path) + 1;
        result = malloc(len);
        if (result) {
            strcpy(result, base_path);
        }
    } else {
        len = strlen(base_path) + 10;
        result = malloc(len);
        if (result) {
            snprintf(result, len, "%s.%03x", base_path, zone);
        }
    }

    return result;
}

char* ftn_bso_get_point_path(const char* base_path, const struct ftn_address* address) {
    char* result;
    size_t len;

    if (!base_path || !address || address->point == 0) {
        return NULL;
    }

    len = strlen(base_path) + 20;
    result = malloc(len);
    if (result) {
        snprintf(result, len, "%s/%08x.pnt", base_path,
                 (unsigned int)((address->net << 16) | address->node));
    }

    return result;
}

ftn_bso_error_t ftn_bso_ensure_directory(const char* path) {
    struct stat st;

    if (!path) {
        return BSO_ERROR_INVALID_PATH;
    }

    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return BSO_OK;
        } else {
            return BSO_ERROR_INVALID_PATH;
        }
    }

    if (mkdir(path, 0755) == 0) {
        logf_debug("Created BSO directory: %s", path);
        return BSO_OK;
    }

    if (errno == EEXIST) {
        return BSO_OK;
    }

    logf_error("Failed to create BSO directory %s: %s", path, strerror(errno));
    return BSO_ERROR_PERMISSION;
}

char* ftn_bso_address_to_hex(const struct ftn_address* addr) {
    char* result;

    if (!addr) {
        return NULL;
    }

    result = malloc(9);
    if (result) {
        snprintf(result, 9, "%08x",
                 (unsigned int)((addr->net << 16) | addr->node));
    }

    return result;
}

ftn_bso_error_t ftn_bso_hex_to_address(const char* hex_str, struct ftn_address* addr) {
    unsigned int hex_value;
    char* endptr;

    if (!hex_str || !addr) {
        return BSO_ERROR_INVALID_ADDRESS;
    }

    if (strlen(hex_str) != 8) {
        return BSO_ERROR_INVALID_ADDRESS;
    }

    hex_value = strtoul(hex_str, &endptr, 16);
    if (*endptr != '\0') {
        return BSO_ERROR_INVALID_ADDRESS;
    }

    addr->net = (hex_value >> 16) & 0xFFFF;
    addr->node = hex_value & 0xFFFF;
    addr->point = 0;
    addr->zone = 0;
    addr->domain = NULL;

    return BSO_OK;
}

char* ftn_bso_get_flow_filename(const struct ftn_address* addr, const char* flavor, const char* extension) {
    char* hex_addr;
    char* result;
    size_t len;

    if (!addr || !extension) {
        return NULL;
    }

    hex_addr = ftn_bso_address_to_hex(addr);
    if (!hex_addr) {
        return NULL;
    }

    len = strlen(hex_addr) + strlen(extension) + 10;
    result = malloc(len);
    if (result) {
        if (flavor && strlen(flavor) > 0) {
            snprintf(result, len, "%c%s.%s", flavor[0], hex_addr, extension);
        } else {
            snprintf(result, len, "%s.%s", hex_addr, extension);
        }
    }

    free(hex_addr);
    return result;
}

ftn_bso_error_t ftn_bso_scan_directory(const char* path, ftn_bso_directory_t* directory) {
    DIR* dir;
    struct dirent* entry;
    struct stat st;
    char full_path[1024];

    if (!path || !directory) {
        return BSO_ERROR_INVALID_PATH;
    }

    memset(directory, 0, sizeof(ftn_bso_directory_t));

    dir = opendir(path);
    if (!dir) {
        logf_error("Cannot open directory %s: %s", path, strerror(errno));
        return BSO_ERROR_NOT_FOUND;
    }

    directory->capacity = 100;
    directory->entries = malloc(directory->capacity * sizeof(ftn_bso_entry_t));
    if (!directory->entries) {
        closedir(dir);
        return BSO_ERROR_MEMORY;
    }

    directory->count = 0;

    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Expand capacity if needed */
        if (directory->count >= directory->capacity) {
            ftn_bso_entry_t* new_entries;
            directory->capacity *= 2;
            new_entries = realloc(directory->entries, directory->capacity * sizeof(ftn_bso_entry_t));
            if (!new_entries) {
                ftn_bso_directory_free(directory);
                closedir(dir);
                return BSO_ERROR_MEMORY;
            }
            directory->entries = new_entries;
        }

        /* Build full path */
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        /* Get file stats */
        if (stat(full_path, &st) != 0) {
            continue;
        }

        /* Fill entry */
        {
            ftn_bso_entry_t* bso_entry = &directory->entries[directory->count];

            bso_entry->filename = malloc(strlen(entry->d_name) + 1);
            bso_entry->full_path = malloc(strlen(full_path) + 1);

            if (!bso_entry->filename || !bso_entry->full_path) {
                if (bso_entry->filename) free(bso_entry->filename);
                if (bso_entry->full_path) free(bso_entry->full_path);
                continue;
            }

            strcpy(bso_entry->filename, entry->d_name);
            strcpy(bso_entry->full_path, full_path);
            bso_entry->mtime = st.st_mtime;
            bso_entry->size = st.st_size;
            bso_entry->is_directory = S_ISDIR(st.st_mode);

            directory->count++;
        }
    }

    closedir(dir);
    logf_debug("Scanned directory %s: found %zu entries", path, directory->count);
    return BSO_OK;
}

void ftn_bso_directory_free(ftn_bso_directory_t* directory) {
    size_t i;

    if (!directory) {
        return;
    }

    if (directory->entries) {
        for (i = 0; i < directory->count; i++) {
            if (directory->entries[i].filename) {
                free(directory->entries[i].filename);
            }
            if (directory->entries[i].full_path) {
                free(directory->entries[i].full_path);
            }
        }
        free(directory->entries);
    }

    memset(directory, 0, sizeof(ftn_bso_directory_t));
}

int ftn_bso_is_valid_outbound(const char* path) {
    struct stat st;

    if (!path) {
        return 0;
    }

    if (stat(path, &st) != 0) {
        return 0;
    }

    return S_ISDIR(st.st_mode);
}

int ftn_bso_is_flow_file(const char* filename) {
    size_t len;
    const char* ext;

    if (!filename) {
        return 0;
    }

    len = strlen(filename);
    if (len < 4) {
        return 0;
    }

    ext = filename + len - 3;

    /* Check for .flo, .out, .hut, .cut, .dut, .iut extensions */
    if (strcasecmp(ext, "flo") == 0 || strcasecmp(ext, "out") == 0) {
        return 1;
    }

    /* Check for flavored extensions like .hut, .cut, etc. */
    if (len >= 4) {
        char flavor = tolower(filename[0]);
        if ((flavor == 'h' || flavor == 'c' || flavor == 'd' || flavor == 'i') &&
            (strcasecmp(ext, "out") == 0 || strcasecmp(ext, "flo") == 0)) {
            return 1;
        }
    }

    return 0;
}

int ftn_bso_is_control_file(const char* filename) {
    size_t len;
    const char* ext;

    if (!filename) {
        return 0;
    }

    len = strlen(filename);
    if (len < 4) {
        return 0;
    }

    ext = filename + len - 3;

    /* Check for .bsy, .csy, .hld, .try extensions */
    if (strcasecmp(ext, "bsy") == 0 || strcasecmp(ext, "csy") == 0 ||
        strcasecmp(ext, "hld") == 0 || strcasecmp(ext, "try") == 0) {
        return 1;
    }

    return 0;
}

const char* ftn_bso_error_string(ftn_bso_error_t error) {
    switch (error) {
        case BSO_OK:
            return "Success";
        case BSO_ERROR_INVALID_PATH:
            return "Invalid path";
        case BSO_ERROR_PERMISSION:
            return "Permission denied";
        case BSO_ERROR_NOT_FOUND:
            return "Not found";
        case BSO_ERROR_INVALID_ADDRESS:
            return "Invalid address";
        case BSO_ERROR_BUSY:
            return "Resource busy";
        case BSO_ERROR_MEMORY:
            return "Out of memory";
        case BSO_ERROR_FILE_IO:
            return "File I/O error";
        default:
            return "Unknown error";
    }
}

ftn_bso_error_t ftn_bso_create_directories(const char* path) {
    char* path_copy;
    char* p;
    struct stat st;

    if (!path) {
        return BSO_ERROR_INVALID_PATH;
    }

    path_copy = malloc(strlen(path) + 1);
    if (!path_copy) {
        return BSO_ERROR_MEMORY;
    }
    strcpy(path_copy, path);

    /* Create directories recursively */
    for (p = path_copy + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';

            if (stat(path_copy, &st) != 0) {
                if (mkdir(path_copy, 0755) != 0 && errno != EEXIST) {
                    free(path_copy);
                    return BSO_ERROR_PERMISSION;
                }
            }

            *p = '/';
        }
    }

    /* Create final directory */
    if (stat(path_copy, &st) != 0) {
        if (mkdir(path_copy, 0755) != 0 && errno != EEXIST) {
            free(path_copy);
            return BSO_ERROR_PERMISSION;
        }
    }

    free(path_copy);
    return BSO_OK;
}

time_t ftn_bso_get_file_mtime(const char* filepath) {
    struct stat st;

    if (!filepath || stat(filepath, &st) != 0) {
        return 0;
    }

    return st.st_mtime;
}

size_t ftn_bso_get_file_size(const char* filepath) {
    struct stat st;

    if (!filepath || stat(filepath, &st) != 0) {
        return 0;
    }

    return st.st_size;
}

ftn_bso_error_t ftn_bso_scan_filtered(const char* path, ftn_bso_filter_func_t filter, void* user_data, ftn_bso_directory_t* directory) {
    ftn_bso_directory_t temp_directory;
    ftn_bso_error_t result;
    size_t i;
    size_t filtered_count;

    if (!path || !directory) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* First scan all entries */
    result = ftn_bso_scan_directory(path, &temp_directory);
    if (result != BSO_OK) {
        return result;
    }

    /* Count filtered entries */
    filtered_count = 0;
    for (i = 0; i < temp_directory.count; i++) {
        if (!filter || filter(temp_directory.entries[i].filename, user_data)) {
            filtered_count++;
        }
    }

    /* Initialize result directory */
    memset(directory, 0, sizeof(ftn_bso_directory_t));
    if (filtered_count > 0) {
        directory->entries = malloc(filtered_count * sizeof(ftn_bso_entry_t));
        if (!directory->entries) {
            ftn_bso_directory_free(&temp_directory);
            return BSO_ERROR_MEMORY;
        }
        directory->capacity = filtered_count;
    }

    /* Copy filtered entries */
    directory->count = 0;
    for (i = 0; i < temp_directory.count; i++) {
        if (!filter || filter(temp_directory.entries[i].filename, user_data)) {
            directory->entries[directory->count] = temp_directory.entries[i];
            /* Clear the source to prevent double-free */
            temp_directory.entries[i].filename = NULL;
            temp_directory.entries[i].full_path = NULL;
            directory->count++;
        }
    }

    ftn_bso_directory_free(&temp_directory);
    return BSO_OK;
}