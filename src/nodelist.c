/*
 * libFTN - Nodelist Implementation
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "ftn.h"
#include <ctype.h>
#include <string.h>

const char* ftn_node_type_to_string(ftn_node_type_t type) {
    switch (type) {
        case FTN_NODE_ZONE: return "Zone";
        case FTN_NODE_REGION: return "Region";
        case FTN_NODE_HOST: return "Host";
        case FTN_NODE_HUB: return "Hub";
        case FTN_NODE_PVT: return "Pvt";
        case FTN_NODE_HOLD: return "Hold";
        case FTN_NODE_DOWN: return "Down";
        case FTN_NODE_NORMAL:
        default: return "Node";
    }
}

ftn_node_type_t ftn_node_type_from_string(const char* str) {
    if (!str || !*str) return FTN_NODE_NORMAL;
    if (strcmp(str, "Zone") == 0) return FTN_NODE_ZONE;
    if (strcmp(str, "Region") == 0) return FTN_NODE_REGION;
    if (strcmp(str, "Host") == 0) return FTN_NODE_HOST;
    if (strcmp(str, "Hub") == 0) return FTN_NODE_HUB;
    if (strcmp(str, "Pvt") == 0) return FTN_NODE_PVT;
    if (strcmp(str, "Hold") == 0) return FTN_NODE_HOLD;
    if (strcmp(str, "Down") == 0) return FTN_NODE_DOWN;
    return FTN_NODE_NORMAL;
}

ftn_nodelist_entry_t* ftn_nodelist_entry_new(void) {
    ftn_nodelist_entry_t* entry = malloc(sizeof(ftn_nodelist_entry_t));
    if (!entry) return NULL;
    
    memset(entry, 0, sizeof(ftn_nodelist_entry_t));
    return entry;
}

void ftn_nodelist_entry_free(ftn_nodelist_entry_t* entry) {
    if (!entry) return;
    
    if (entry->name) free(entry->name);
    if (entry->location) free(entry->location);
    if (entry->sysop) free(entry->sysop);
    if (entry->phone) free(entry->phone);
    if (entry->speed) free(entry->speed);
    if (entry->flags) free(entry->flags);
    
    free(entry);
}

static char* replace_underscores(const char* str) {
    char* result;
    char* p;
    
    if (!str) return NULL;
    
    result = strdup(str);
    if (!result) return NULL;
    
    for (p = result; *p; p++) {
        if (*p == '_') *p = ' ';
    }
    
    return result;
}

ftn_error_t ftn_nodelist_parse_line(const char* line, ftn_nodelist_entry_t* entry) {
    char* work_line;
    char* fields[8];
    int field_count = 0;
    char* p;
    
    if (!line || !entry) return FTN_ERROR_INVALID;
    
    /* Skip comment lines */
    if (line[0] == ';') return FTN_ERROR_PARSE;
    
    work_line = strdup(line);
    if (!work_line) return FTN_ERROR_NOMEM;
    
    ftn_trim(work_line);
    if (strlen(work_line) == 0) {
        free(work_line);
        return FTN_ERROR_PARSE;
    }
    
    /* Parse comma-separated fields */
    p = work_line;
    fields[field_count++] = p;
    
    while (*p && field_count < 8) {
        if (*p == ',') {
            *p = '\0';
            p++;
            fields[field_count++] = p;
        } else {
            p++;
        }
    }
    
    /* Must have at least 7 fields */
    if (field_count < 7) {
        free(work_line);
        return FTN_ERROR_PARSE;
    }
    
    /* Parse keyword (field 0) */
    ftn_trim(fields[0]);
    entry->type = ftn_node_type_from_string(fields[0]);
    
    /* Parse number (field 1) */
    ftn_trim(fields[1]);
    if (entry->type == FTN_NODE_ZONE) {
        entry->address.zone = atoi(fields[1]);
        entry->address.net = 0;
        entry->address.node = 0;
    } else if (entry->type == FTN_NODE_REGION) {
        entry->address.net = atoi(fields[1]);
        entry->address.node = 0;
    } else if (entry->type == FTN_NODE_HOST) {
        entry->address.net = atoi(fields[1]);
        entry->address.node = 0;
    } else {
        entry->address.node = atoi(fields[1]);
    }
    
    /* Parse name (field 2) */
    ftn_trim(fields[2]);
    entry->name = replace_underscores(fields[2]);
    
    /* Parse location (field 3) */
    ftn_trim(fields[3]);
    entry->location = replace_underscores(fields[3]);
    
    /* Parse sysop (field 4) */
    ftn_trim(fields[4]);
    entry->sysop = replace_underscores(fields[4]);
    
    /* Parse phone (field 5) */
    ftn_trim(fields[5]);
    entry->phone = strdup(fields[5]);
    
    /* Parse speed (field 6) */
    ftn_trim(fields[6]);
    entry->speed = strdup(fields[6]);
    
    /* Parse flags (field 7) - optional */
    if (field_count >= 8) {
        ftn_trim(fields[7]);
        entry->flags = strdup(fields[7]);
    } else {
        entry->flags = strdup("");
    }
    
    free(work_line);
    return FTN_OK;
}

ftn_error_t ftn_nodelist_parse_comment(const char* line, ftn_comment_flags_t* flags, char** text) {
    const char* p;
    
    if (!line || !flags || !text) return FTN_ERROR_INVALID;
    
    *flags = FTN_COMMENT_NONE;
    *text = NULL;
    
    if (line[0] != ';') return FTN_ERROR_PARSE;
    
    p = line + 1;
    
    /* Parse interest flags */
    while (*p && !isspace(*p)) {
        switch (*p) {
            case 'S': *flags |= FTN_COMMENT_SYSOP; break;
            case 'U': *flags |= FTN_COMMENT_USER; break;
            case 'F': *flags |= FTN_COMMENT_FIDO; break;
            case 'A': *flags |= FTN_COMMENT_ALL; break;
            case 'E': *flags |= FTN_COMMENT_ERROR; break;
            default: break;
        }
        p++;
    }
    
    /* Skip whitespace */
    while (*p && isspace(*p)) p++;
    
    *text = strdup(p);
    return FTN_OK;
}

static ftn_error_t resize_nodelist_if_needed(ftn_nodelist_t* nodelist) {
    ftn_nodelist_entry_t** new_entries;
    size_t new_capacity;
    
    if (nodelist->count < nodelist->capacity) {
        return FTN_OK;
    }
    
    new_capacity = nodelist->capacity == 0 ? 100 : nodelist->capacity * 2;
    new_entries = realloc(nodelist->entries, new_capacity * sizeof(ftn_nodelist_entry_t*));
    if (!new_entries) {
        return FTN_ERROR_NOMEM;
    }
    
    nodelist->entries = new_entries;
    nodelist->capacity = new_capacity;
    return FTN_OK;
}

ftn_error_t ftn_nodelist_load(const char* filename, ftn_nodelist_t** nodelist) {
    FILE* fp;
    char line[1024];
    ftn_nodelist_t* nl;
    ftn_error_t result;
    unsigned int expected_crc = 0;
    char* crc_pos;
    unsigned int current_zone = 0;
    unsigned int current_net = 0;
    (void)expected_crc; /* Avoid unused variable warning */
    
    if (!filename || !nodelist) return FTN_ERROR_INVALID;
    
    fp = fopen(filename, "r");
    if (!fp) return FTN_ERROR_FILE;
    
    nl = malloc(sizeof(ftn_nodelist_t));
    if (!nl) {
        fclose(fp);
        return FTN_ERROR_NOMEM;
    }
    
    memset(nl, 0, sizeof(ftn_nodelist_t));
    
    /* Read first line to get title and CRC */
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        free(nl);
        return FTN_ERROR_FILE;
    }
    
    ftn_trim(line);
    nl->title = strdup(line);
    
    /* Extract CRC from title line */
    crc_pos = strrchr(line, ':');
    if (crc_pos && crc_pos[1] == ' ') {
        expected_crc = atoi(crc_pos + 2);
        nl->crc = expected_crc;
    }
    
    /* Read and parse data lines */
    while (fgets(line, sizeof(line), fp)) {
        ftn_nodelist_entry_t* new_entry;
        
        ftn_trim(line);
        
        /* Skip empty lines and comments */
        if (strlen(line) == 0 || line[0] == ';') continue;
        
        /* Create new entry */
        new_entry = ftn_nodelist_entry_new();
        if (!new_entry) {
            ftn_nodelist_free(nl);
            fclose(fp);
            return FTN_ERROR_NOMEM;
        }
        
        /* Parse the line */
        result = ftn_nodelist_parse_line(line, new_entry);
        if (result != FTN_OK) {
            ftn_nodelist_entry_free(new_entry);
            continue;
        }
        
        /* Update zone/net context */
        if (new_entry->type == FTN_NODE_ZONE) {
            current_zone = new_entry->address.zone;
            current_net = 0;
        } else if (new_entry->type == FTN_NODE_REGION || new_entry->type == FTN_NODE_HOST) {
            current_net = new_entry->address.net;
        }
        
        /* Set zone/net for normal nodes */
        if (new_entry->address.zone == 0) new_entry->address.zone = current_zone;
        if (new_entry->address.net == 0 && new_entry->type != FTN_NODE_ZONE) {
            new_entry->address.net = current_net;
        }
        
        /* Resize array if needed */
        result = resize_nodelist_if_needed(nl);
        if (result != FTN_OK) {
            ftn_nodelist_entry_free(new_entry);
            ftn_nodelist_free(nl);
            fclose(fp);
            return result;
        }
        
        /* Add entry to nodelist */
        nl->entries[nl->count] = new_entry;
        nl->count++;
    }
    
    fclose(fp);
    
    /* Verify CRC if we have one (skip for now) */
    /*
    if (expected_crc > 0) {
        result = ftn_nodelist_verify_crc(filename, expected_crc);
        if (result != FTN_OK) {
            ftn_nodelist_free(nl);
            return result;
        }
    }
    */
    
    *nodelist = nl;
    return FTN_OK;
}

void ftn_nodelist_free(ftn_nodelist_t* nodelist) {
    size_t i;
    
    if (!nodelist) return;
    
    if (nodelist->title) free(nodelist->title);
    
    if (nodelist->entries) {
        for (i = 0; i < nodelist->count; i++) {
            ftn_nodelist_entry_free(nodelist->entries[i]);
        }
        free(nodelist->entries);
    }
    
    free(nodelist);
}

const char* ftn_inet_protocol_to_string(ftn_inet_protocol_t protocol) {
    switch (protocol) {
        case FTN_INET_IBN: return "Binkp";
        case FTN_INET_IFC: return "ifcico";
        case FTN_INET_IFT: return "FTP";
        case FTN_INET_ITN: return "Telnet";
        case FTN_INET_IVM: return "Vmodem";
        default: return "Unknown";
    }
}

unsigned int ftn_inet_protocol_default_port(ftn_inet_protocol_t protocol) {
    switch (protocol) {
        case FTN_INET_IBN: return 24554;
        case FTN_INET_IFC: return 60179;
        case FTN_INET_IFT: return 21;
        case FTN_INET_ITN: return 23;
        case FTN_INET_IVM: return 3141;
        default: return 0;
    }
}

static ftn_inet_protocol_t ftn_inet_protocol_from_string(const char* str) {
    if (strcmp(str, "IBN") == 0) return FTN_INET_IBN;
    if (strcmp(str, "IFC") == 0) return FTN_INET_IFC;
    if (strcmp(str, "IFT") == 0) return FTN_INET_IFT;
    if (strcmp(str, "ITN") == 0) return FTN_INET_ITN;
    if (strcmp(str, "IVM") == 0) return FTN_INET_IVM;
    return FTN_INET_IBN; /* default */
}

static int ftn_is_inet_protocol_flag(const char* flag) {
    return (strncmp(flag, "IBN", 3) == 0 ||
            strncmp(flag, "IFC", 3) == 0 ||
            strncmp(flag, "IFT", 3) == 0 ||
            strncmp(flag, "ITN", 3) == 0 ||
            strncmp(flag, "IVM", 3) == 0);
}

static int ftn_is_inet_flag(const char* flag) {
    return (ftn_is_inet_protocol_flag(flag) ||
            strncmp(flag, "INA:", 4) == 0 ||
            strcmp(flag, "INO4") == 0 ||
            strcmp(flag, "ICM") == 0);
}

size_t ftn_nodelist_parse_inet_flags(const char* flags, ftn_inet_service_t** services) {
    char* work_flags;
    char* flag;
    char* saveptr;
    size_t count = 0;
    size_t capacity = 4;
    ftn_inet_service_t* service_list;
    char* default_hostname = NULL;
    
    if (!flags || !services) return 0;
    
    *services = NULL;
    
    service_list = malloc(capacity * sizeof(ftn_inet_service_t));
    if (!service_list) return 0;
    
    work_flags = strdup(flags);
    if (!work_flags) {
        free(service_list);
        return 0;
    }
    
    /* First pass: look for INA flag to get default hostname */
    flag = strtok_r(work_flags, ",", &saveptr);
    while (flag) {
        ftn_trim(flag);
        
        if (strncmp(flag, "INA:", 4) == 0) {
            default_hostname = strdup(flag + 4);
            break;
        }
        
        flag = strtok_r(NULL, ",", &saveptr);
    }
    
    /* Reset for second pass */
    free(work_flags);
    work_flags = strdup(flags);
    if (!work_flags) {
        if (default_hostname) free(default_hostname);
        free(service_list);
        return 0;
    }
    
    /* Second pass: parse protocol flags */
    flag = strtok_r(work_flags, ",", &saveptr);
    while (flag) {
        char* colon_pos;
        char protocol_name[4];
        
        ftn_trim(flag);
        
        if (ftn_is_inet_protocol_flag(flag)) {
            ftn_inet_service_t* new_service;
            
            /* Resize array if needed */
            if (count >= capacity) {
                ftn_inet_service_t* temp;
                capacity *= 2;
                temp = realloc(service_list, capacity * sizeof(ftn_inet_service_t));
                if (!temp) {
                    break;
                }
                service_list = temp;
            }
            
            new_service = &service_list[count];
            memset(new_service, 0, sizeof(ftn_inet_service_t));
            
            /* Extract protocol name */
            strncpy(protocol_name, flag, 3);
            protocol_name[3] = '\0';
            new_service->protocol = ftn_inet_protocol_from_string(protocol_name);
            
            /* Parse hostname and port */
            colon_pos = strchr(flag, ':');
            if (colon_pos) {
                char* second_colon;
                colon_pos++; /* Skip the first colon */
                
                second_colon = strchr(colon_pos, ':');
                if (second_colon) {
                    /* Format: PROTOCOL:hostname:port */
                    *second_colon = '\0';
                    new_service->hostname = strdup(colon_pos);
                    new_service->port = atoi(second_colon + 1);
                    new_service->has_port = 1;
                } else {
                    /* Check if it's just a port number */
                    char* endptr;
                    unsigned int port_num = strtoul(colon_pos, &endptr, 10);
                    if (*endptr == '\0' && port_num > 0) {
                        /* Format: PROTOCOL:port */
                        new_service->hostname = default_hostname ? strdup(default_hostname) : NULL;
                        new_service->port = port_num;
                        new_service->has_port = 1;
                    } else {
                        /* Format: PROTOCOL:hostname */
                        new_service->hostname = strdup(colon_pos);
                        new_service->port = ftn_inet_protocol_default_port(new_service->protocol);
                        new_service->has_port = 0;
                    }
                }
            } else {
                /* Format: PROTOCOL */
                new_service->hostname = default_hostname ? strdup(default_hostname) : NULL;
                new_service->port = ftn_inet_protocol_default_port(new_service->protocol);
                new_service->has_port = 0;
            }
            
            count++;
        }
        
        flag = strtok_r(NULL, ",", &saveptr);
    }
    
    if (default_hostname) free(default_hostname);
    free(work_flags);
    
    if (count == 0) {
        free(service_list);
        return 0;
    }
    
    *services = service_list;
    return count;
}

void ftn_nodelist_free_inet_services(ftn_inet_service_t* services, size_t count) {
    size_t i;
    
    if (!services) return;
    
    for (i = 0; i < count; i++) {
        if (services[i].hostname) {
            free(services[i].hostname);
        }
    }
    
    free(services);
}

char* ftn_nodelist_filter_inet_flags(const char* flags) {
    char* work_flags;
    char* flag;
    char* saveptr;
    char* result;
    size_t result_len = 0;
    size_t result_capacity = 256;
    int first_flag = 1;
    
    if (!flags) return NULL;
    
    result = malloc(result_capacity);
    if (!result) return NULL;
    result[0] = '\0';
    
    work_flags = strdup(flags);
    if (!work_flags) {
        free(result);
        return NULL;
    }
    
    /* Parse through flags and keep only non-Internet ones */
    flag = strtok_r(work_flags, ",", &saveptr);
    while (flag) {
        ftn_trim(flag);
        
        if (!ftn_is_inet_flag(flag)) {
            size_t flag_len = strlen(flag);
            size_t needed_len = result_len + flag_len + (first_flag ? 0 : 1) + 1;
            
            /* Resize result buffer if needed */
            if (needed_len > result_capacity) {
                char* temp;
                result_capacity = needed_len * 2;
                temp = realloc(result, result_capacity);
                if (!temp) {
                    free(result);
                    free(work_flags);
                    return NULL;
                }
                result = temp;
            }
            
            /* Add comma separator if not first flag */
            if (!first_flag) {
                strcat(result, ",");
                result_len++;
            }
            
            /* Add the flag */
            strcat(result, flag);
            result_len += flag_len;
            first_flag = 0;
        }
        
        flag = strtok_r(NULL, ",", &saveptr);
    }
    
    free(work_flags);
    return result;
}
