#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include "ftn/control.h"
#include "ftn/log.h"

/* Address structure (should match the one in bso.c) */
typedef struct ftn_address {
    int zone;
    int net;
    int node;
    int point;
    char* domain;
} ftn_address_t;

ftn_bso_error_t ftn_control_file_init(ftn_control_file_t* control) {
    if (!control) {
        return BSO_ERROR_INVALID_PATH;
    }

    memset(control, 0, sizeof(ftn_control_file_t));
    return BSO_OK;
}

void ftn_control_file_free(ftn_control_file_t* control) {
    if (!control) {
        return;
    }

    if (control->address) {
        if (control->address->domain) {
            free(control->address->domain);
        }
        free(control->address);
        control->address = NULL;
    }

    if (control->control_path) {
        free(control->control_path);
        control->control_path = NULL;
    }

    if (control->pid_info) {
        free(control->pid_info);
        control->pid_info = NULL;
    }

    if (control->reason) {
        free(control->reason);
        control->reason = NULL;
    }

    memset(control, 0, sizeof(ftn_control_file_t));
}

char* ftn_control_get_filename(const struct ftn_address* addr, ftn_control_type_t type) {
    char* hex_addr;
    char* result;
    const char* ext;
    size_t len;

    if (!addr) {
        return NULL;
    }

    hex_addr = ftn_bso_address_to_hex(addr);
    if (!hex_addr) {
        return NULL;
    }

    ext = ftn_control_type_extension(type);
    if (!ext) {
        free(hex_addr);
        return NULL;
    }

    len = strlen(hex_addr) + strlen(ext) + 2;
    result = malloc(len);
    if (result) {
        snprintf(result, len, "%s.%s", hex_addr, ext);
    }

    free(hex_addr);
    return result;
}

char* ftn_control_get_filepath(const struct ftn_address* addr, const char* outbound, ftn_control_type_t type) {
    char* filename;
    char* result;
    size_t len;

    if (!addr || !outbound) {
        return NULL;
    }

    filename = ftn_control_get_filename(addr, type);
    if (!filename) {
        return NULL;
    }

    len = strlen(outbound) + strlen(filename) + 2;
    result = malloc(len);
    if (result) {
        snprintf(result, len, "%s/%s", outbound, filename);
    }

    free(filename);
    return result;
}

ftn_bso_error_t ftn_control_atomic_create(const char* filepath, const char* content) {
    int fd;
    size_t content_len;
    ssize_t written;

    if (!filepath || !content) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Use O_CREAT | O_EXCL to ensure atomic creation */
    fd = open(filepath, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd == -1) {
        if (errno == EEXIST) {
            return BSO_ERROR_BUSY;
        }
        logf_error("Cannot create control file %s: %s", filepath, strerror(errno));
        return BSO_ERROR_FILE_IO;
    }

    content_len = strlen(content);
    written = write(fd, content, content_len);
    close(fd);

    if (written != (ssize_t)content_len) {
        /* Failed to write, clean up */
        unlink(filepath);
        return BSO_ERROR_FILE_IO;
    }

    logf_debug("Created control file: %s", filepath);
    return BSO_OK;
}

ftn_bso_error_t ftn_control_atomic_remove(const char* filepath) {
    if (!filepath) {
        return BSO_ERROR_INVALID_PATH;
    }

    if (unlink(filepath) != 0) {
        if (errno == ENOENT) {
            return BSO_OK;
        }
        logf_error("Cannot remove control file %s: %s", filepath, strerror(errno));
        return BSO_ERROR_FILE_IO;
    }

    logf_debug("Removed control file: %s", filepath);
    return BSO_OK;
}

ftn_bso_error_t ftn_control_read_content(const char* filepath, char** content) {
    FILE* file;
    struct stat st;
    char* buffer;
    size_t bytes_read;

    if (!filepath || !content) {
        return BSO_ERROR_INVALID_PATH;
    }

    *content = NULL;

    if (stat(filepath, &st) != 0) {
        return BSO_ERROR_NOT_FOUND;
    }

    file = fopen(filepath, "r");
    if (!file) {
        return BSO_ERROR_FILE_IO;
    }

    buffer = malloc(st.st_size + 1);
    if (!buffer) {
        fclose(file);
        return BSO_ERROR_MEMORY;
    }

    bytes_read = fread(buffer, 1, st.st_size, file);
    fclose(file);

    if (bytes_read != (size_t)st.st_size) {
        free(buffer);
        return BSO_ERROR_FILE_IO;
    }

    buffer[bytes_read] = '\0';
    *content = buffer;
    return BSO_OK;
}

ftn_bso_error_t ftn_control_generate_bsy_content(char** content) {
    char* buffer;
    pid_t pid;

    if (!content) {
        return BSO_ERROR_INVALID_PATH;
    }

    pid = getpid();
    buffer = malloc(64);
    if (!buffer) {
        return BSO_ERROR_MEMORY;
    }

    snprintf(buffer, 64, "fnmailer %d\n", (int)pid);
    *content = buffer;
    return BSO_OK;
}

ftn_bso_error_t ftn_control_generate_hld_content(time_t until, const char* reason, char** content) {
    char* buffer;
    size_t len;

    if (!content) {
        return BSO_ERROR_INVALID_PATH;
    }

    len = 64 + (reason ? strlen(reason) : 0);
    buffer = malloc(len);
    if (!buffer) {
        return BSO_ERROR_MEMORY;
    }

    if (reason) {
        snprintf(buffer, len, "%ld %s\n", (long)until, reason);
    } else {
        snprintf(buffer, len, "%ld\n", (long)until);
    }

    *content = buffer;
    return BSO_OK;
}

ftn_bso_error_t ftn_control_generate_try_content(int attempt_count, const char* reason, char** content) {
    char* buffer;
    size_t len;

    if (!content) {
        return BSO_ERROR_INVALID_PATH;
    }

    len = 64 + (reason ? strlen(reason) : 0);
    buffer = malloc(len);
    if (!buffer) {
        return BSO_ERROR_MEMORY;
    }

    if (reason) {
        snprintf(buffer, len, "%d %s\n", attempt_count, reason);
    } else {
        snprintf(buffer, len, "%d\n", attempt_count);
    }

    *content = buffer;
    return BSO_OK;
}

ftn_bso_error_t ftn_control_acquire_bsy(const struct ftn_address* addr, const char* outbound, ftn_control_file_t* control) {
    char* filepath;
    char* content;
    ftn_bso_error_t result;

    if (!addr || !outbound || !control) {
        return BSO_ERROR_INVALID_PATH;
    }

    ftn_control_file_init(control);

    filepath = ftn_control_get_filepath(addr, outbound, CONTROL_TYPE_BSY);
    if (!filepath) {
        return BSO_ERROR_MEMORY;
    }

    /* Generate BSY content */
    result = ftn_control_generate_bsy_content(&content);
    if (result != BSO_OK) {
        free(filepath);
        return result;
    }

    /* Attempt atomic creation */
    result = ftn_control_atomic_create(filepath, content);
    if (result == BSO_OK) {
        /* Success - populate control structure */
        control->type = CONTROL_TYPE_BSY;
        control->control_path = filepath;
        control->created = time(NULL);
        control->pid_info = content;

        /* Copy address */
        control->address = malloc(sizeof(ftn_address_t));
        if (control->address) {
            memcpy(control->address, addr, sizeof(ftn_address_t));
            control->address->domain = NULL;
            if (addr->domain) {
                control->address->domain = malloc(strlen(addr->domain) + 1);
                if (control->address->domain) {
                    strcpy(control->address->domain, addr->domain);
                }
            }
        }

        logf_info("Acquired BSY lock for %d:%d/%d.%d", addr->zone, addr->net, addr->node, addr->point);
    } else {
        free(filepath);
        free(content);

        if (result == BSO_ERROR_BUSY) {
            logf_debug("BSY lock already exists for %d:%d/%d.%d", addr->zone, addr->net, addr->node, addr->point);
        }
    }

    return result;
}

ftn_bso_error_t ftn_control_release_bsy(const ftn_control_file_t* control) {
    if (!control || control->type != CONTROL_TYPE_BSY || !control->control_path) {
        return BSO_ERROR_INVALID_PATH;
    }

    {
        ftn_bso_error_t result = ftn_control_atomic_remove(control->control_path);
        if (result == BSO_OK && control->address) {
            logf_info("Released BSY lock for %d:%d/%d.%d",
                         control->address->zone, control->address->net,
                         control->address->node, control->address->point);
        }
        return result;
    }
}

ftn_bso_error_t ftn_control_check_bsy(const struct ftn_address* addr, const char* outbound, ftn_control_file_t* control) {
    char* filepath;
    char* content;
    ftn_bso_error_t result;

    if (!addr || !outbound || !control) {
        return BSO_ERROR_INVALID_PATH;
    }

    filepath = ftn_control_get_filepath(addr, outbound, CONTROL_TYPE_BSY);
    if (!filepath) {
        return BSO_ERROR_MEMORY;
    }

    result = ftn_control_read_content(filepath, &content);
    if (result == BSO_OK) {
        ftn_control_file_init(control);
        control->type = CONTROL_TYPE_BSY;
        control->control_path = filepath;
        control->created = ftn_bso_get_file_mtime(filepath);
        control->pid_info = content;

        /* Copy address */
        control->address = malloc(sizeof(ftn_address_t));
        if (control->address) {
            memcpy(control->address, addr, sizeof(ftn_address_t));
            control->address->domain = NULL;
            if (addr->domain) {
                control->address->domain = malloc(strlen(addr->domain) + 1);
                if (control->address->domain) {
                    strcpy(control->address->domain, addr->domain);
                }
            }
        }
    } else {
        free(filepath);
    }

    return result;
}

ftn_bso_error_t ftn_control_create_hld(const struct ftn_address* addr, const char* outbound, time_t until, const char* reason) {
    char* filepath;
    char* content;
    ftn_bso_error_t result;

    if (!addr || !outbound) {
        return BSO_ERROR_INVALID_PATH;
    }

    filepath = ftn_control_get_filepath(addr, outbound, CONTROL_TYPE_HLD);
    if (!filepath) {
        return BSO_ERROR_MEMORY;
    }

    result = ftn_control_generate_hld_content(until, reason, &content);
    if (result == BSO_OK) {
        result = ftn_control_atomic_create(filepath, content);
        if (result == BSO_OK) {
            logf_info("Created HLD file for %d:%d/%d.%d until %ld",
                         addr->zone, addr->net, addr->node, addr->point, (long)until);
        }
        free(content);
    }

    free(filepath);
    return result;
}

ftn_bso_error_t ftn_control_check_hld(const struct ftn_address* addr, const char* outbound, time_t* until, char** reason) {
    char* filepath;
    char* content;
    ftn_bso_error_t result;

    if (!addr || !outbound) {
        return BSO_ERROR_INVALID_PATH;
    }

    filepath = ftn_control_get_filepath(addr, outbound, CONTROL_TYPE_HLD);
    if (!filepath) {
        return BSO_ERROR_MEMORY;
    }

    result = ftn_control_read_content(filepath, &content);
    if (result == BSO_OK) {
        result = ftn_control_parse_hld_content(content, until, reason);
        free(content);
    }

    free(filepath);
    return result;
}

ftn_bso_error_t ftn_control_parse_hld_content(const char* content, time_t* until, char** reason) {
    char* line_copy;
    char* token;
    char* saveptr;
    long timestamp;

    if (!content || !until) {
        return BSO_ERROR_INVALID_PATH;
    }

    line_copy = malloc(strlen(content) + 1);
    if (!line_copy) {
        return BSO_ERROR_MEMORY;
    }
    strcpy(line_copy, content);

    /* Parse timestamp */
    token = strtok_r(line_copy, " \t\n", &saveptr);
    if (!token) {
        free(line_copy);
        return BSO_ERROR_INVALID_PATH;
    }

    timestamp = strtol(token, NULL, 10);
    *until = (time_t)timestamp;

    /* Parse reason (optional) */
    if (reason) {
        token = strtok_r(NULL, "\n", &saveptr);
        if (token) {
            *reason = malloc(strlen(token) + 1);
            if (*reason) {
                strcpy(*reason, token);
            }
        } else {
            *reason = NULL;
        }
    }

    free(line_copy);
    return BSO_OK;
}

const char* ftn_control_type_string(ftn_control_type_t type) {
    switch (type) {
        case CONTROL_TYPE_BSY:
            return "busy";
        case CONTROL_TYPE_CSY:
            return "call";
        case CONTROL_TYPE_HLD:
            return "hold";
        case CONTROL_TYPE_TRY:
            return "try";
        default:
            return "unknown";
    }
}

const char* ftn_control_type_extension(ftn_control_type_t type) {
    switch (type) {
        case CONTROL_TYPE_BSY:
            return "bsy";
        case CONTROL_TYPE_CSY:
            return "csy";
        case CONTROL_TYPE_HLD:
            return "hld";
        case CONTROL_TYPE_TRY:
            return "try";
        default:
            return NULL;
    }
}

int ftn_control_type_from_extension(const char* extension, ftn_control_type_t* type) {
    if (!extension || !type) {
        return 0;
    }

    if (strcasecmp(extension, "bsy") == 0) {
        *type = CONTROL_TYPE_BSY;
        return 1;
    } else if (strcasecmp(extension, "csy") == 0) {
        *type = CONTROL_TYPE_CSY;
        return 1;
    } else if (strcasecmp(extension, "hld") == 0) {
        *type = CONTROL_TYPE_HLD;
        return 1;
    } else if (strcasecmp(extension, "try") == 0) {
        *type = CONTROL_TYPE_TRY;
        return 1;
    }

    return 0;
}

int ftn_control_is_stale(const ftn_control_file_t* control, time_t max_age) {
    time_t now;

    if (!control) {
        return 0;
    }

    now = time(NULL);
    return (now - control->created) > max_age;
}

ftn_bso_error_t ftn_control_cleanup_stale(const char* outbound, time_t max_age) {
    DIR* dir;
    struct dirent* entry;
    char filepath[1024];
    struct stat st;
    time_t now;
    int cleaned = 0;

    if (!outbound) {
        return BSO_ERROR_INVALID_PATH;
    }

    dir = opendir(outbound);
    if (!dir) {
        return BSO_ERROR_NOT_FOUND;
    }

    now = time(NULL);

    while ((entry = readdir(dir)) != NULL) {
        /* Check if it's a control file */
        if (!ftn_bso_is_control_file(entry->d_name)) {
            continue;
        }

        snprintf(filepath, sizeof(filepath), "%s/%s", outbound, entry->d_name);

        if (stat(filepath, &st) != 0) {
            continue;
        }

        /* Check if file is stale */
        if ((now - st.st_mtime) > max_age) {
            if (unlink(filepath) == 0) {
                logf_debug("Cleaned up stale control file: %s", entry->d_name);
                cleaned++;
            }
        }
    }

    closedir(dir);

    if (cleaned > 0) {
        logf_info("Cleaned up %d stale control files in %s", cleaned, outbound);
    }

    return BSO_OK;
}

ftn_bso_error_t ftn_control_lock_init(ftn_control_lock_t* lock) {
    if (!lock) {
        return BSO_ERROR_INVALID_PATH;
    }

    memset(lock, 0, sizeof(ftn_control_lock_t));
    return BSO_OK;
}

void ftn_control_lock_free(ftn_control_lock_t* lock) {
    if (!lock) {
        return;
    }

    if (lock->address) {
        if (lock->address->domain) {
            free(lock->address->domain);
        }
        free(lock->address);
        lock->address = NULL;
    }

    if (lock->outbound_path) {
        free(lock->outbound_path);
        lock->outbound_path = NULL;
    }

    if (lock->bsy_file) {
        ftn_control_file_free(lock->bsy_file);
        free(lock->bsy_file);
        lock->bsy_file = NULL;
    }

    memset(lock, 0, sizeof(ftn_control_lock_t));
}

ftn_bso_error_t ftn_control_acquire_lock(const struct ftn_address* addr, const char* outbound, ftn_control_lock_t* lock) {
    ftn_bso_error_t result;

    if (!addr || !outbound || !lock) {
        return BSO_ERROR_INVALID_PATH;
    }

    ftn_control_lock_init(lock);

    /* Allocate BSY control file structure */
    lock->bsy_file = malloc(sizeof(ftn_control_file_t));
    if (!lock->bsy_file) {
        return BSO_ERROR_MEMORY;
    }

    /* Attempt to acquire BSY lock */
    result = ftn_control_acquire_bsy(addr, outbound, lock->bsy_file);
    if (result != BSO_OK) {
        free(lock->bsy_file);
        lock->bsy_file = NULL;
        return result;
    }

    /* Copy address and outbound path */
    lock->address = malloc(sizeof(ftn_address_t));
    if (lock->address) {
        memcpy(lock->address, addr, sizeof(ftn_address_t));
        lock->address->domain = NULL;
        if (addr->domain) {
            lock->address->domain = malloc(strlen(addr->domain) + 1);
            if (lock->address->domain) {
                strcpy(lock->address->domain, addr->domain);
            }
        }
    }

    lock->outbound_path = malloc(strlen(outbound) + 1);
    if (lock->outbound_path) {
        strcpy(lock->outbound_path, outbound);
    }

    lock->lock_time = time(NULL);

    return BSO_OK;
}

ftn_bso_error_t ftn_control_release_lock(ftn_control_lock_t* lock) {
    ftn_bso_error_t result;

    if (!lock || !lock->bsy_file) {
        return BSO_ERROR_INVALID_PATH;
    }

    result = ftn_control_release_bsy(lock->bsy_file);
    ftn_control_lock_free(lock);

    return result;
}