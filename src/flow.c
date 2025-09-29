#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include "ftn/flow.h"
#include "ftn/log.h"

/* Address structure (should match the one in bso.c) */
typedef struct ftn_address {
    int zone;
    int net;
    int node;
    int point;
    char* domain;
} ftn_address_t;

ftn_bso_error_t ftn_flow_file_init(ftn_flow_file_t* flow) {
    if (!flow) {
        return BSO_ERROR_INVALID_PATH;
    }

    memset(flow, 0, sizeof(ftn_flow_file_t));
    return BSO_OK;
}

void ftn_flow_file_free(ftn_flow_file_t* flow) {
    size_t i;

    if (!flow) {
        return;
    }

    if (flow->filepath) {
        free(flow->filepath);
        flow->filepath = NULL;
    }

    if (flow->filename) {
        free(flow->filename);
        flow->filename = NULL;
    }

    if (flow->target_address) {
        if (flow->target_address->domain) {
            free(flow->target_address->domain);
        }
        free(flow->target_address);
        flow->target_address = NULL;
    }

    if (flow->entries) {
        for (i = 0; i < flow->file_count; i++) {
            ftn_flow_reference_entry_free(&flow->entries[i]);
        }
        free(flow->entries);
        flow->entries = NULL;
    }

    memset(flow, 0, sizeof(ftn_flow_file_t));
}

ftn_bso_error_t ftn_flow_parse_filename(const char* filename, ftn_flow_type_t* type, ftn_flow_flavor_t* flavor, struct ftn_address* address) {
    size_t len;
    const char* ext;
    char hex_part[9];
    char flavor_char;
    ftn_bso_error_t result;

    if (!filename || !type || !flavor || !address) {
        return BSO_ERROR_INVALID_PATH;
    }

    len = strlen(filename);
    if (len < 4) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Get extension */
    ext = filename + len - 3;

    /* Determine type from extension */
    if (strcasecmp(ext, "out") == 0) {
        *type = FLOW_TYPE_NETMAIL;
    } else if (strcasecmp(ext, "flo") == 0) {
        *type = FLOW_TYPE_REFERENCE;
    } else {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Parse flavor and address */
    if (len == 12) {
        /* 8 hex digits + .ext (normal format) */
        *flavor = FLOW_FLAVOR_NORMAL;
        strncpy(hex_part, filename, 8);
        hex_part[8] = '\0';
    } else if (len == 13) {
        /* flavor + 8 hex digits + .ext */
        flavor_char = tolower(filename[0]);
        switch (flavor_char) {
            case 'i':
                *flavor = FLOW_FLAVOR_IMMEDIATE;
                break;
            case 'c':
                *flavor = FLOW_FLAVOR_CONTINUOUS;
                break;
            case 'd':
                *flavor = FLOW_FLAVOR_DIRECT;
                break;
            case 'h':
                *flavor = FLOW_FLAVOR_HOLD;
                break;
            default:
                return BSO_ERROR_INVALID_PATH;
        }
        strncpy(hex_part, filename + 1, 8);
        hex_part[8] = '\0';
    } else {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Convert hex to address */
    result = ftn_bso_hex_to_address(hex_part, address);
    if (result != BSO_OK) {
        return result;
    }

    logf_debug("Parsed flow file %s: type=%s, flavor=%s, address=%d:%d/%d.%d",
                  filename, ftn_flow_type_string(*type), ftn_flow_flavor_string(*flavor),
                  address->zone, address->net, address->node, address->point);

    return BSO_OK;
}

ftn_bso_error_t ftn_flow_list_init(ftn_flow_list_t* list) {
    if (!list) {
        return BSO_ERROR_INVALID_PATH;
    }

    memset(list, 0, sizeof(ftn_flow_list_t));
    list->capacity = 10;
    list->flows = malloc(list->capacity * sizeof(ftn_flow_file_t));
    if (!list->flows) {
        return BSO_ERROR_MEMORY;
    }

    return BSO_OK;
}

void ftn_flow_list_free(ftn_flow_list_t* list) {
    size_t i;

    if (!list) {
        return;
    }

    if (list->flows) {
        for (i = 0; i < list->count; i++) {
            ftn_flow_file_free(&list->flows[i]);
        }
        free(list->flows);
        list->flows = NULL;
    }

    memset(list, 0, sizeof(ftn_flow_list_t));
}

ftn_bso_error_t ftn_flow_list_add(ftn_flow_list_t* list, const ftn_flow_file_t* flow) {
    if (!list || !flow) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Expand capacity if needed */
    if (list->count >= list->capacity) {
        ftn_flow_file_t* new_flows;
        list->capacity *= 2;
        new_flows = realloc(list->flows, list->capacity * sizeof(ftn_flow_file_t));
        if (!new_flows) {
            return BSO_ERROR_MEMORY;
        }
        list->flows = new_flows;
    }

    /* Copy flow (shallow copy, caller retains ownership) */
    memcpy(&list->flows[list->count], flow, sizeof(ftn_flow_file_t));
    list->count++;

    return BSO_OK;
}

static int flow_priority_compare(const void* a, const void* b) {
    const ftn_flow_file_t* flow_a = (const ftn_flow_file_t*)a;
    const ftn_flow_file_t* flow_b = (const ftn_flow_file_t*)b;
    int prio_a = ftn_flow_flavor_priority(flow_a->flavor);
    int prio_b = ftn_flow_flavor_priority(flow_b->flavor);

    /* Lower priority value = higher priority */
    if (prio_a != prio_b) {
        return prio_a - prio_b;
    }

    /* Same priority, sort by timestamp */
    if (flow_a->timestamp < flow_b->timestamp) {
        return -1;
    } else if (flow_a->timestamp > flow_b->timestamp) {
        return 1;
    }

    return 0;
}

ftn_bso_error_t ftn_flow_list_sort_by_priority(ftn_flow_list_t* list) {
    if (!list || !list->flows) {
        return BSO_ERROR_INVALID_PATH;
    }

    qsort(list->flows, list->count, sizeof(ftn_flow_file_t), flow_priority_compare);
    return BSO_OK;
}

ftn_bso_error_t ftn_flow_load_file(const char* filepath, ftn_flow_file_t* flow) {
    const char* filename;
    struct stat st;

    if (!filepath || !flow) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Get file stats */
    if (stat(filepath, &st) != 0) {
        logf_error("Cannot stat flow file %s: %s", filepath, strerror(errno));
        return BSO_ERROR_NOT_FOUND;
    }

    /* Initialize flow */
    ftn_flow_file_init(flow);

    /* Set basic properties */
    flow->filepath = malloc(strlen(filepath) + 1);
    if (!flow->filepath) {
        return BSO_ERROR_MEMORY;
    }
    strcpy(flow->filepath, filepath);

    filename = strrchr(filepath, '/');
    if (filename) {
        filename++;
    } else {
        filename = filepath;
    }

    flow->filename = malloc(strlen(filename) + 1);
    if (!flow->filename) {
        ftn_flow_file_free(flow);
        return BSO_ERROR_MEMORY;
    }
    strcpy(flow->filename, filename);

    flow->timestamp = st.st_mtime;

    /* Allocate address structure */
    flow->target_address = malloc(sizeof(ftn_address_t));
    if (!flow->target_address) {
        ftn_flow_file_free(flow);
        return BSO_ERROR_MEMORY;
    }
    memset(flow->target_address, 0, sizeof(ftn_address_t));

    /* Parse filename to get type, flavor, and address */
    {
        ftn_bso_error_t result = ftn_flow_parse_filename(filename, &flow->type, &flow->flavor, flow->target_address);
        if (result != BSO_OK) {
            ftn_flow_file_free(flow);
            return result;
        }
    }

    /* Load file contents based on type */
    if (flow->type == FLOW_TYPE_REFERENCE) {
        return ftn_flow_parse_reference_file(filepath, flow);
    } else {
        return ftn_flow_process_netmail_file(filepath, flow);
    }
}

ftn_bso_error_t ftn_flow_parse_reference_file(const char* filepath, ftn_flow_file_t* flow) {
    FILE* file;
    char line[1024];
    ftn_reference_entry_t entry;
    ftn_bso_error_t result;

    if (!filepath || !flow) {
        return BSO_ERROR_INVALID_PATH;
    }

    file = fopen(filepath, "r");
    if (!file) {
        logf_error("Cannot open reference file %s: %s", filepath, strerror(errno));
        return BSO_ERROR_NOT_FOUND;
    }

    flow->file_count = 0;
    flow->entry_capacity = 10;
    flow->entries = malloc(flow->entry_capacity * sizeof(ftn_reference_entry_t));
    if (!flow->entries) {
        fclose(file);
        return BSO_ERROR_MEMORY;
    }

    while (fgets(line, sizeof(line), file)) {
        /* Remove trailing newline */
        {
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
            }
        }

        /* Skip empty lines */
        if (strlen(line) == 0) {
            continue;
        }

        /* Parse reference entry */
        result = ftn_flow_parse_reference_line(line, &entry);
        if (result != BSO_OK) {
            logf_warning("Invalid reference line in %s: %s", filepath, line);
            continue;
        }

        /* Add to flow */
        result = ftn_flow_add_reference_entry(flow, &entry);
        if (result != BSO_OK) {
            ftn_flow_reference_entry_free(&entry);
            fclose(file);
            return result;
        }
    }

    fclose(file);
    logf_debug("Loaded reference file %s with %zu entries", filepath, flow->file_count);
    return BSO_OK;
}

ftn_bso_error_t ftn_flow_process_netmail_file(const char* filepath, ftn_flow_file_t* flow) {
    ftn_reference_entry_t entry;
    ftn_bso_error_t result;

    if (!filepath || !flow) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* For netmail files, create a single entry pointing to the file itself */
    memset(&entry, 0, sizeof(ftn_reference_entry_t));

    entry.filepath = malloc(strlen(filepath) + 1);
    if (!entry.filepath) {
        return BSO_ERROR_MEMORY;
    }
    strcpy(entry.filepath, filepath);

    entry.directive = REF_DIRECTIVE_SEND;
    entry.processed = 0;
    entry.timestamp = ftn_bso_get_file_mtime(filepath);
    entry.file_size = ftn_bso_get_file_size(filepath);

    /* Initialize entries array */
    flow->entry_capacity = 1;
    flow->entries = malloc(flow->entry_capacity * sizeof(ftn_reference_entry_t));
    if (!flow->entries) {
        ftn_flow_reference_entry_free(&entry);
        return BSO_ERROR_MEMORY;
    }

    result = ftn_flow_add_reference_entry(flow, &entry);
    if (result != BSO_OK) {
        ftn_flow_reference_entry_free(&entry);
        return result;
    }

    logf_debug("Processed netmail file %s", filepath);
    return BSO_OK;
}

ftn_bso_error_t ftn_flow_parse_reference_line(const char* line, ftn_reference_entry_t* entry) {
    const char* filepath;
    size_t len;

    if (!line || !entry) {
        return BSO_ERROR_INVALID_PATH;
    }

    memset(entry, 0, sizeof(ftn_reference_entry_t));
    len = strlen(line);

    if (len == 0) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Parse directive */
    switch (line[0]) {
        case '#':
            entry->directive = REF_DIRECTIVE_TRUNCATE;
            filepath = line + 1;
            break;
        case '^':
        case '-':
            entry->directive = REF_DIRECTIVE_DELETE;
            filepath = line + 1;
            break;
        case '~':
        case '!':
            entry->directive = REF_DIRECTIVE_SKIP;
            filepath = line + 1;
            break;
        case '@':
            entry->directive = REF_DIRECTIVE_SEND;
            filepath = line + 1;
            break;
        default:
            entry->directive = REF_DIRECTIVE_NONE;
            filepath = line;
            break;
    }

    /* Skip whitespace */
    while (*filepath && isspace((unsigned char)*filepath)) {
        filepath++;
    }

    if (*filepath == '\0') {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Copy filepath */
    entry->filepath = malloc(strlen(filepath) + 1);
    if (!entry->filepath) {
        return BSO_ERROR_MEMORY;
    }
    strcpy(entry->filepath, filepath);

    /* Get file stats if file exists */
    entry->timestamp = ftn_bso_get_file_mtime(entry->filepath);
    entry->file_size = ftn_bso_get_file_size(entry->filepath);

    return BSO_OK;
}

ftn_bso_error_t ftn_flow_add_reference_entry(ftn_flow_file_t* flow, const ftn_reference_entry_t* entry) {
    if (!flow || !entry) {
        return BSO_ERROR_INVALID_PATH;
    }

    /* Expand capacity if needed */
    if (flow->file_count >= flow->entry_capacity) {
        ftn_reference_entry_t* new_entries;
        flow->entry_capacity *= 2;
        new_entries = realloc(flow->entries, flow->entry_capacity * sizeof(ftn_reference_entry_t));
        if (!new_entries) {
            return BSO_ERROR_MEMORY;
        }
        flow->entries = new_entries;
    }

    /* Copy entry */
    memcpy(&flow->entries[flow->file_count], entry, sizeof(ftn_reference_entry_t));
    flow->file_count++;

    return BSO_OK;
}

void ftn_flow_reference_entry_free(ftn_reference_entry_t* entry) {
    if (!entry) {
        return;
    }

    if (entry->filepath) {
        free(entry->filepath);
        entry->filepath = NULL;
    }

    memset(entry, 0, sizeof(ftn_reference_entry_t));
}

const char* ftn_flow_type_string(ftn_flow_type_t type) {
    switch (type) {
        case FLOW_TYPE_NETMAIL:
            return "netmail";
        case FLOW_TYPE_REFERENCE:
            return "reference";
        default:
            return "unknown";
    }
}

const char* ftn_flow_flavor_string(ftn_flow_flavor_t flavor) {
    switch (flavor) {
        case FLOW_FLAVOR_IMMEDIATE:
            return "immediate";
        case FLOW_FLAVOR_CONTINUOUS:
            return "continuous";
        case FLOW_FLAVOR_DIRECT:
            return "direct";
        case FLOW_FLAVOR_NORMAL:
            return "normal";
        case FLOW_FLAVOR_HOLD:
            return "hold";
        default:
            return "unknown";
    }
}

const char* ftn_flow_directive_string(ftn_ref_directive_t directive) {
    switch (directive) {
        case REF_DIRECTIVE_NONE:
            return "send";
        case REF_DIRECTIVE_TRUNCATE:
            return "truncate";
        case REF_DIRECTIVE_DELETE:
            return "delete";
        case REF_DIRECTIVE_SKIP:
            return "skip";
        case REF_DIRECTIVE_SEND:
            return "send";
        default:
            return "unknown";
    }
}

int ftn_flow_flavor_priority(ftn_flow_flavor_t flavor) {
    switch (flavor) {
        case FLOW_FLAVOR_IMMEDIATE:
            return 1;
        case FLOW_FLAVOR_CONTINUOUS:
            return 2;
        case FLOW_FLAVOR_DIRECT:
            return 3;
        case FLOW_FLAVOR_NORMAL:
            return 4;
        case FLOW_FLAVOR_HOLD:
            return 5;
        default:
            return 999;
    }
}

int ftn_flow_is_valid_reference_file(const char* filepath) {
    FILE* file;
    char line[1024];
    int valid_lines = 0;

    if (!filepath) {
        return 0;
    }

    file = fopen(filepath, "r");
    if (!file) {
        return 0;
    }

    /* Check first few lines for valid format */
    while (fgets(line, sizeof(line), file) && valid_lines < 10) {
        ftn_reference_entry_t entry;
        if (ftn_flow_parse_reference_line(line, &entry) == BSO_OK) {
            valid_lines++;
            ftn_flow_reference_entry_free(&entry);
        }
    }

    fclose(file);
    return valid_lines > 0;
}

int ftn_flow_is_valid_netmail_file(const char* filepath) {
    struct stat st;

    if (!filepath) {
        return 0;
    }

    /* Basic check - file exists and is readable */
    if (stat(filepath, &st) != 0) {
        return 0;
    }

    return S_ISREG(st.st_mode);
}

ftn_bso_error_t ftn_flow_validate_file_exists(const ftn_reference_entry_t* entry) {
    struct stat st;

    if (!entry || !entry->filepath) {
        return BSO_ERROR_INVALID_PATH;
    }

    if (stat(entry->filepath, &st) != 0) {
        return BSO_ERROR_NOT_FOUND;
    }

    if (!S_ISREG(st.st_mode)) {
        return BSO_ERROR_INVALID_PATH;
    }

    return BSO_OK;
}