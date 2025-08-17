/*
 * libFTN - RFC822 Message Format Implementation
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include <ftn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>


#define RFC822_INITIAL_HEADERS 10
#define RFC822_HEADER_GROWTH 5

/* Create a new RFC822 message */
rfc822_message_t* rfc822_message_new(void) {
    rfc822_message_t* message = malloc(sizeof(rfc822_message_t));
    if (!message) return NULL;
    
    message->headers = malloc(sizeof(rfc822_header_t*) * RFC822_INITIAL_HEADERS);
    if (!message->headers) {
        free(message);
        return NULL;
    }
    
    message->header_count = 0;
    message->header_capacity = RFC822_INITIAL_HEADERS;
    message->body = NULL;
    
    return message;
}

/* Free an RFC822 message */
void rfc822_message_free(rfc822_message_t* message) {
    size_t i;
    
    if (!message) return;
    
    for (i = 0; i < message->header_count; i++) {
        if (message->headers[i]) {
            free(message->headers[i]->name);
            free(message->headers[i]->value);
            free(message->headers[i]);
        }
    }
    
    free(message->headers);
    free(message->body);
    free(message);
}

/* Add a header to an RFC822 message */
ftn_error_t rfc822_message_add_header(rfc822_message_t* message, const char* name, const char* value) {
    rfc822_header_t* header;
    
    if (!message || !name || !value) return FTN_ERROR_INVALID_PARAMETER;
    
    /* Grow headers array if needed */
    if (message->header_count >= message->header_capacity) {
        size_t new_capacity = message->header_capacity + RFC822_HEADER_GROWTH;
        rfc822_header_t** new_headers = realloc(message->headers, 
                                                sizeof(rfc822_header_t*) * new_capacity);
        if (!new_headers) return FTN_ERROR_NOMEM;
        
        message->headers = new_headers;
        message->header_capacity = new_capacity;
    }
    
    /* Create new header */
    header = malloc(sizeof(rfc822_header_t));
    if (!header) return FTN_ERROR_NOMEM;
    
    header->name = malloc(strlen(name) + 1);
    header->value = malloc(strlen(value) + 1);
    
    if (!header->name || !header->value) {
        free(header->name);
        free(header->value);
        free(header);
        return FTN_ERROR_NOMEM;
    }
    
    strcpy(header->name, name);
    strcpy(header->value, value);
    
    message->headers[message->header_count++] = header;
    
    return FTN_OK;
}

/* Get header value by name */
const char* rfc822_message_get_header(const rfc822_message_t* message, const char* name) {
    size_t i;
    
    if (!message || !name) return NULL;
    
    for (i = 0; i < message->header_count; i++) {
        if (message->headers[i] && strcasecmp(message->headers[i]->name, name) == 0) {
            return message->headers[i]->value;
        }
    }
    
    return NULL;
}

/* Set header value (replace if exists, add if not) */
ftn_error_t rfc822_message_set_header(rfc822_message_t* message, const char* name, const char* value) {
    size_t i;
    char* new_value;
    
    if (!message || !name || !value) return FTN_ERROR_INVALID_PARAMETER;
    
    /* Look for existing header */
    for (i = 0; i < message->header_count; i++) {
        if (message->headers[i] && strcasecmp(message->headers[i]->name, name) == 0) {
            new_value = malloc(strlen(value) + 1);
            if (!new_value) return FTN_ERROR_NOMEM;
            
            strcpy(new_value, value);
            free(message->headers[i]->value);
            message->headers[i]->value = new_value;
            return FTN_OK;
        }
    }
    
    /* Header doesn't exist, add it */
    return rfc822_message_add_header(message, name, value);
}

/* Remove header by name */
ftn_error_t rfc822_message_remove_header(rfc822_message_t* message, const char* name) {
    size_t i, j;
    
    if (!message || !name) return FTN_ERROR_INVALID_PARAMETER;
    
    for (i = 0; i < message->header_count; i++) {
        if (message->headers[i] && strcasecmp(message->headers[i]->name, name) == 0) {
            free(message->headers[i]->name);
            free(message->headers[i]->value);
            free(message->headers[i]);
            
            /* Shift remaining headers down */
            for (j = i; j < message->header_count - 1; j++) {
                message->headers[j] = message->headers[j + 1];
            }
            message->header_count--;
            return FTN_OK;
        }
    }
    
    return FTN_ERROR_NOTFOUND;
}

/* Set message body */
ftn_error_t rfc822_message_set_body(rfc822_message_t* message, const char* body) {
    char* new_body;
    
    if (!message) return FTN_ERROR_INVALID_PARAMETER;
    
    if (body) {
        new_body = malloc(strlen(body) + 1);
        if (!new_body) return FTN_ERROR_NOMEM;
        strcpy(new_body, body);
    } else {
        new_body = NULL;
    }
    
    free(message->body);
    message->body = new_body;
    
    return FTN_OK;
}

/* Convert RFC822 text to CRLF line endings */
static char* convert_to_crlf(const char* text) {
    size_t len, new_len, i, j;
    char* result;
    
    if (!text) return NULL;
    
    len = strlen(text);
    new_len = len;
    
    /* Count LF characters that aren't preceded by CR */
    for (i = 0; i < len; i++) {
        if (text[i] == '\n' && (i == 0 || text[i-1] != '\r')) {
            new_len++;
        }
    }
    
    result = malloc(new_len + 1);
    if (!result) return NULL;
    
    j = 0;
    for (i = 0; i < len; i++) {
        if (text[i] == '\n' && (i == 0 || text[i-1] != '\r')) {
            result[j++] = '\r';
        }
        result[j++] = text[i];
    }
    result[j] = '\0';
    
    return result;
}

/* Convert CRLF line endings to LF */
static char* convert_from_crlf(const char* text) {
    size_t len, i, j;
    char* result;
    
    if (!text) return NULL;
    
    len = strlen(text);
    result = malloc(len + 1);
    if (!result) return NULL;
    
    j = 0;
    for (i = 0; i < len; i++) {
        if (text[i] == '\r' && i + 1 < len && text[i + 1] == '\n') {
            /* Skip CR, let LF through */
            continue;
        }
        result[j++] = text[i];
    }
    result[j] = '\0';
    
    return result;
}

/* Generate RFC822 text from message */
char* rfc822_message_to_text(const rfc822_message_t* message) {
    size_t total_size = 0;
    size_t i;
    char* result;
    char* pos;
    char* body_crlf;
    
    if (!message) return NULL;
    
    /* Calculate required size */
    for (i = 0; i < message->header_count; i++) {
        if (message->headers[i]) {
            total_size += strlen(message->headers[i]->name) + 2; /* name + ": " */
            total_size += strlen(message->headers[i]->value) + 2; /* value + CRLF */
        }
    }
    
    total_size += 2; /* Empty line between headers and body */
    
    if (message->body) {
        body_crlf = convert_to_crlf(message->body);
        if (body_crlf) {
            total_size += strlen(body_crlf);
        }
    } else {
        body_crlf = NULL;
    }
    
    result = malloc(total_size + 1);
    if (!result) {
        free(body_crlf);
        return NULL;
    }
    
    pos = result;
    
    /* Write headers */
    for (i = 0; i < message->header_count; i++) {
        if (message->headers[i]) {
            strcpy(pos, message->headers[i]->name);
            pos += strlen(message->headers[i]->name);
            strcpy(pos, ": ");
            pos += 2;
            strcpy(pos, message->headers[i]->value);
            pos += strlen(message->headers[i]->value);
            strcpy(pos, "\r\n");
            pos += 2;
        }
    }
    
    /* Empty line */
    strcpy(pos, "\r\n");
    pos += 2;
    
    /* Write body */
    if (body_crlf) {
        strcpy(pos, body_crlf);
        free(body_crlf);
    }
    
    return result;
}

/* Parse RFC822 message from text */
ftn_error_t rfc822_message_parse(const char* text, rfc822_message_t** message) {
    rfc822_message_t* msg;
    const char* pos;
    const char* line_start;
    const char* body_start = NULL;
    char* line;
    char* colon_pos;
    char* name;
    char* value;
    size_t line_len;
    ftn_error_t error;
    
    if (!text || !message) return FTN_ERROR_INVALID_PARAMETER;
    
    msg = rfc822_message_new();
    if (!msg) return FTN_ERROR_NOMEM;
    
    pos = text;
    
    /* Parse headers */
    while (*pos) {
        line_start = pos;
        
        /* Find end of line */
        while (*pos && *pos != '\r' && *pos != '\n') pos++;
        
        line_len = pos - line_start;
        
        /* Skip CRLF */
        if (*pos == '\r') pos++;
        if (*pos == '\n') pos++;
        
        /* Empty line marks end of headers */
        if (line_len == 0) {
            body_start = pos;
            break;
        }
        
        /* Copy line */
        line = malloc(line_len + 1);
        if (!line) {
            rfc822_message_free(msg);
            return FTN_ERROR_NOMEM;
        }
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';
        
        /* Find colon separator */
        colon_pos = strchr(line, ':');
        if (!colon_pos) {
            free(line);
            continue; /* Skip malformed header */
        }
        
        /* Extract name and value */
        *colon_pos = '\0';
        name = line;
        value = colon_pos + 1;
        
        /* Trim whitespace from value */
        while (*value == ' ' || *value == '\t') value++;
        
        /* Add header */
        error = rfc822_message_add_header(msg, name, value);
        free(line);
        
        if (error != FTN_OK) {
            rfc822_message_free(msg);
            return error;
        }
    }
    
    /* Set body if present */
    if (body_start && *body_start) {
        char* body = convert_from_crlf(body_start);
        if (body) {
            error = rfc822_message_set_body(msg, body);
            free(body);
            if (error != FTN_OK) {
                rfc822_message_free(msg);
                return error;
            }
        }
    }
    
    *message = msg;
    return FTN_OK;
}

/* Convert FTN address to Internet FQDN format (P.F.N.Z.DOMAIN) */
static char* ftn_address_to_fqdn(const ftn_address_t* addr, const char* domain) {
    char* result;
    size_t domain_len;
    size_t total_len;
    
    if (!addr || !domain) return NULL;
    
    domain_len = strlen(domain);
    
    if (addr->point > 0) {
        /* Include point: P.F.N.Z.DOMAIN */
        total_len = 64 + domain_len; /* Conservative estimate */
        result = malloc(total_len);
        if (result) {
            snprintf(result, total_len, "%u.%u.%u.%u.%s", 
                     addr->point, addr->node, addr->net, addr->zone, domain);
        }
    } else {
        /* No point: F.N.Z.DOMAIN */
        total_len = 64 + domain_len; /* Conservative estimate */
        result = malloc(total_len);
        if (result) {
            snprintf(result, total_len, "%u.%u.%u.%s", 
                     addr->node, addr->net, addr->zone, domain);
        }
    }
    
    return result;
}

/* Format FTN address for RFC822 */
char* ftn_address_to_rfc822(const ftn_address_t* addr, const char* name, const char* domain) {
    char* fqdn;
    char* result;
    char* username;
    size_t total_len;
    size_t username_len;
    size_t i;
    
    if (!addr || !domain) return NULL;
    
    fqdn = ftn_address_to_fqdn(addr, domain);
    if (!fqdn) return NULL;
    
    /* Create username from name (convert to lowercase, replace spaces/special chars) */
    if (name && *name) {
        username_len = strlen(name);
        username = malloc(username_len + 1);
        if (!username) {
            free(fqdn);
            return NULL;
        }
        
        /* Convert name to valid email username */
        for (i = 0; i < username_len; i++) {
            char c = name[i];
            if (c >= 'A' && c <= 'Z') {
                username[i] = c + 32; /* Convert to lowercase */
            } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
                username[i] = c; /* Keep valid characters */
            } else {
                username[i] = '_'; /* Replace invalid characters with underscore */
            }
        }
        username[username_len] = '\0';
        
        total_len = strlen(name) + strlen(username) + strlen(fqdn) + 16; /* Extra space for formatting */
        result = malloc(total_len);
        if (result) {
            snprintf(result, total_len, "\"%s\" <%s@%s>", name, username, fqdn);
        }
        free(username);
    } else {
        /* No name provided, use generic username */
        total_len = strlen(fqdn) + 16; /* Extra space for "user@" */
        result = malloc(total_len);
        if (result) {
            snprintf(result, total_len, "user@%s", fqdn);
        }
    }
    
    free(fqdn);
    return result;
}

/* Parse email address or FQDN back to FTN address (user@P.F.N.Z.DOMAIN or P.F.N.Z.DOMAIN) */
static ftn_error_t fqdn_to_ftn_address(const char* addr_str, const char* domain, ftn_address_t* addr) {
    char* fqdn_copy;
    char* dot_pos;
    char* domain_pos;
    char* at_pos;
    char* fqdn_start;
    char* parts[4]; /* point, node, net, zone */
    int part_count = 0;
    
    if (!addr_str || !domain || !addr) return FTN_ERROR_INVALID_PARAMETER;
    
    /* Check if this is an email address (user@fqdn) or just FQDN */
    at_pos = strchr(addr_str, '@');
    if (at_pos) {
        /* Email format - extract FQDN part after @ */
        fqdn_start = at_pos + 1;
    } else {
        /* Just FQDN */
        fqdn_start = (char*)addr_str;
    }
    
    /* Check if FQDN ends with expected domain */
    domain_pos = strstr(fqdn_start, domain);
    if (!domain_pos || domain_pos == fqdn_start) {
        return FTN_ERROR_INVALID_FORMAT;
    }
    
    /* Check that domain is at the end and preceded by a dot */
    if (strcmp(domain_pos, domain) != 0 || *(domain_pos - 1) != '.') {
        return FTN_ERROR_INVALID_FORMAT;
    }
    
    /* Copy FQDN without domain part */
    fqdn_copy = malloc(domain_pos - fqdn_start);
    if (!fqdn_copy) return FTN_ERROR_NOMEM;
    memcpy(fqdn_copy, fqdn_start, domain_pos - fqdn_start - 1); /* -1 to exclude the dot */
    fqdn_copy[domain_pos - fqdn_start - 1] = '\0';
    
    /* Split by dots */
    parts[part_count] = fqdn_copy;
    for (dot_pos = fqdn_copy; *dot_pos; dot_pos++) {
        if (*dot_pos == '.') {
            *dot_pos = '\0';
            if (part_count < 3) {
                parts[++part_count] = dot_pos + 1;
            }
        }
    }
    part_count++;
    
    /* Initialize address */
    memset(addr, 0, sizeof(ftn_address_t));
    
    if (part_count == 3) {
        /* F.N.Z format (no point) */
        addr->node = atoi(parts[0]);
        addr->net = atoi(parts[1]);
        addr->zone = atoi(parts[2]);
        addr->point = 0;
    } else if (part_count == 4) {
        /* P.F.N.Z format (with point) */
        addr->point = atoi(parts[0]);
        addr->node = atoi(parts[1]);
        addr->net = atoi(parts[2]);
        addr->zone = atoi(parts[3]);
    } else {
        free(fqdn_copy);
        return FTN_ERROR_INVALID_FORMAT;
    }
    
    /* Validate parsed values */
    if (addr->zone == 0 || addr->net == 0 || addr->node == 0) {
        free(fqdn_copy);
        return FTN_ERROR_INVALID_FORMAT;
    }
    
    free(fqdn_copy);
    return FTN_OK;
}

/* Parse RFC822 address to extract FTN address */
ftn_error_t rfc822_address_to_ftn(const char* rfc_addr, const char* domain, ftn_address_t* addr, char** name) {
    const char* bracket_start;
    const char* bracket_end;
    const char* at_pos;
    const char* space_pos;
    char* addr_part;
    char* name_part = NULL;
    char* user_part = NULL;
    ftn_error_t result;
    size_t name_len;
    size_t addr_len;
    size_t user_len;
    
    if (!rfc_addr || !domain || !addr) return FTN_ERROR_INVALID_PARAMETER;
    
    /* Look for angle brackets <address> */
    bracket_start = strchr(rfc_addr, '<');
    bracket_end = strchr(rfc_addr, '>');
    
    if (bracket_start && bracket_end && bracket_end > bracket_start) {
        /* Extract name part if present */
        if (bracket_start > rfc_addr) {
            name_len = bracket_start - rfc_addr;
            while (name_len > 0 && isspace(rfc_addr[name_len - 1])) name_len--;
            
            if (name_len > 0) {
                name_part = malloc(name_len + 1);
                if (name_part) {
                    memcpy(name_part, rfc_addr, name_len);
                    name_part[name_len] = '\0';
                    
                    /* Remove quotes if present */
                    if (name_part[0] == '"' && name_part[name_len - 1] == '"') {
                        memmove(name_part, name_part + 1, name_len - 2);
                        name_part[name_len - 2] = '\0';
                    }
                }
            }
        }
        
        /* Extract address from brackets */
        addr_len = bracket_end - bracket_start - 1;
        addr_part = malloc(addr_len + 1);
        if (!addr_part) {
            free(name_part);
            return FTN_ERROR_NOMEM;
        }
        memcpy(addr_part, bracket_start + 1, addr_len);
        addr_part[addr_len] = '\0';
    } else {
        /* No brackets, check if it's user@fqdn or Name <user@fqdn> format */
        at_pos = strchr(rfc_addr, '@');
        if (!at_pos) {
            return FTN_ERROR_INVALID_FORMAT;
        }
        
        /* Check if there's a space before the @ - indicates Name user@fqdn format */
        space_pos = strrchr(rfc_addr, ' ');
        if (space_pos && space_pos < at_pos) {
            /* Extract name part */
            name_len = space_pos - rfc_addr;
            if (name_len > 0) {
                name_part = malloc(name_len + 1);
                if (name_part) {
                    memcpy(name_part, rfc_addr, name_len);
                    name_part[name_len] = '\0';
                    
                    /* Remove quotes if present */
                    if (name_part[0] == '"' && name_part[name_len - 1] == '"') {
                        memmove(name_part, name_part + 1, name_len - 2);
                        name_part[name_len - 2] = '\0';
                    }
                }
            }
            
            /* Extract address part after space */
            addr_part = malloc(strlen(space_pos + 1) + 1);
            if (!addr_part) {
                free(name_part);
                return FTN_ERROR_NOMEM;
            }
            strcpy(addr_part, space_pos + 1);
        } else {
            /* Simple user@fqdn format */
            addr_part = malloc(strlen(rfc_addr) + 1);
            if (!addr_part) return FTN_ERROR_NOMEM;
            strcpy(addr_part, rfc_addr);
        }
    }
    
    /* Extract user part from address for potential name */
    at_pos = strchr(addr_part, '@');
    if (at_pos && !name_part) {
        /* Use user part as name if no name was specified */
        user_len = at_pos - addr_part;
        if (user_len > 0) {
            user_part = malloc(user_len + 1);
            if (user_part) {
                memcpy(user_part, addr_part, user_len);
                user_part[user_len] = '\0';
                name_part = user_part;
            }
        }
    }
    
    /* Parse FQDN format address */
    result = fqdn_to_ftn_address(addr_part, domain, addr);
    
    free(addr_part);
    
    if (result != FTN_OK) {
        free(name_part);
        return result;
    }
    
    if (name) {
        *name = name_part;
    } else {
        free(name_part);
    }
    
    return FTN_OK;
}

/* Convert FTN timestamp to RFC822 date format */
char* ftn_timestamp_to_rfc822(time_t timestamp) {
    struct tm* tm_info;
    char* result;
    static const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    tm_info = gmtime(&timestamp);
    if (!tm_info) return NULL;
    
    result = malloc(64);
    if (!result) return NULL;
    
    snprintf(result, 64, "%s, %02d %s %04d %02d:%02d:%02d GMT",
             weekdays[tm_info->tm_wday],
             tm_info->tm_mday,
             months[tm_info->tm_mon],
             tm_info->tm_year + 1900,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);
    
    return result;
}

/* Parse RFC822 date to timestamp */
ftn_error_t rfc822_date_to_timestamp(const char* date_str, time_t* timestamp) {
    /* This is a simplified parser - RFC822 date parsing is quite complex */
    struct tm tm_info;
    int day, year, hour, min, sec;
    char month_str[4];
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int month = -1;
    int i;
    
    if (!date_str || !timestamp) return FTN_ERROR_INVALID_PARAMETER;
    
    memset(&tm_info, 0, sizeof(tm_info));
    
    /* Simple format: "DD Mon YYYY HH:MM:SS" */
    if (sscanf(date_str, "%d %3s %d %d:%d:%d", &day, month_str, &year, &hour, &min, &sec) == 6) {
        for (i = 0; i < 12; i++) {
            if (strncasecmp(month_str, months[i], 3) == 0) {
                month = i;
                break;
            }
        }
        
        if (month >= 0) {
            tm_info.tm_mday = day;
            tm_info.tm_mon = month;
            tm_info.tm_year = year - 1900;
            tm_info.tm_hour = hour;
            tm_info.tm_min = min;
            tm_info.tm_sec = sec;
            
            *timestamp = mktime(&tm_info);
            return FTN_OK;
        }
    }
    
    return FTN_ERROR_INVALID_FORMAT;
}

/* Encode text for safe transport (basic implementation) */
char* rfc822_encode_text(const char* text) {
    /* For now, just copy the text - could implement quoted-printable here */
    char* result;
    
    if (!text) return NULL;
    
    result = malloc(strlen(text) + 1);
    if (result) {
        strcpy(result, text);
    }
    return result;
}

/* Decode text (basic implementation) */
char* rfc822_decode_text(const char* text) {
    /* For now, just copy the text - could implement quoted-printable decoding here */
    char* result;
    
    if (!text) return NULL;
    
    result = malloc(strlen(text) + 1);
    if (result) {
        strcpy(result, text);
    }
    return result;
}

/* Convert FTN message to RFC822 */
ftn_error_t ftn_to_rfc822(const ftn_message_t* ftn_msg, const char* domain, rfc822_message_t** rfc_msg) {
    rfc822_message_t* msg;
    char* from_addr;
    char* to_addr;
    char* date_str;
    char buffer[256];
    ftn_error_t error;
    size_t i;
    
    if (!ftn_msg || !domain || !rfc_msg) return FTN_ERROR_INVALID_PARAMETER;
    
    msg = rfc822_message_new();
    if (!msg) return FTN_ERROR_NOMEM;
    
    /* Set From header */
    from_addr = ftn_address_to_rfc822(&ftn_msg->orig_addr, ftn_msg->from_user, domain);
    if (from_addr) {
        error = rfc822_message_add_header(msg, "From", from_addr);
        free(from_addr);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Set To header */
    to_addr = ftn_address_to_rfc822(&ftn_msg->dest_addr, ftn_msg->to_user, domain);
    if (to_addr) {
        error = rfc822_message_add_header(msg, "To", to_addr);
        free(to_addr);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Set Subject header */
    if (ftn_msg->subject) {
        error = rfc822_message_add_header(msg, "Subject", ftn_msg->subject);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Set Date header */
    date_str = ftn_timestamp_to_rfc822(ftn_msg->timestamp);
    if (date_str) {
        error = rfc822_message_add_header(msg, "Date", date_str);
        free(date_str);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Set Message-ID header if MSGID is available */
    if (ftn_msg->msgid) {
        error = rfc822_message_add_header(msg, "Message-ID", ftn_msg->msgid);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Set In-Reply-To header if REPLY is available */
    if (ftn_msg->reply) {
        error = rfc822_message_add_header(msg, "In-Reply-To", ftn_msg->reply);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Add FTN-specific headers */
    snprintf(buffer, sizeof(buffer), "%u:%u/%u.%u", ftn_msg->orig_addr.zone, ftn_msg->orig_addr.net,
             ftn_msg->orig_addr.node, ftn_msg->orig_addr.point);
    error = rfc822_message_add_header(msg, "X-FTN-From", buffer);
    if (error != FTN_OK) goto error_cleanup;
    
    snprintf(buffer, sizeof(buffer), "%u:%u/%u.%u", ftn_msg->dest_addr.zone, ftn_msg->dest_addr.net,
             ftn_msg->dest_addr.node, ftn_msg->dest_addr.point);
    error = rfc822_message_add_header(msg, "X-FTN-To", buffer);
    if (error != FTN_OK) goto error_cleanup;
    
    if (ftn_msg->attributes) {
        snprintf(buffer, sizeof(buffer), "0x%04X", ftn_msg->attributes);
        error = rfc822_message_add_header(msg, "X-FTN-Attributes", buffer);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Add control paragraphs as X-FTN headers */
    for (i = 0; i < ftn_msg->control_count; i++) {
        if (ftn_msg->control_lines[i]) {
            error = rfc822_message_add_header(msg, "X-FTN-Control", ftn_msg->control_lines[i]);
            if (error != FTN_OK) goto error_cleanup;
        }
    }
    
    /* Add SEEN-BY lines */
    for (i = 0; i < ftn_msg->seenby_count; i++) {
        if (ftn_msg->seenby[i]) {
            error = rfc822_message_add_header(msg, "X-FTN-Seen-By", ftn_msg->seenby[i]);
            if (error != FTN_OK) goto error_cleanup;
        }
    }
    
    /* Add PATH lines */
    for (i = 0; i < ftn_msg->path_count; i++) {
        if (ftn_msg->path[i]) {
            error = rfc822_message_add_header(msg, "X-FTN-Path", ftn_msg->path[i]);
            if (error != FTN_OK) goto error_cleanup;
        }
    }
    
    /* Add area for Echomail */
    if (ftn_msg->area) {
        error = rfc822_message_add_header(msg, "X-FTN-Area", ftn_msg->area);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Add origin line */
    if (ftn_msg->origin) {
        error = rfc822_message_add_header(msg, "X-FTN-Origin", ftn_msg->origin);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Add tearline */
    if (ftn_msg->tearline) {
        error = rfc822_message_add_header(msg, "X-FTN-Tearline", ftn_msg->tearline);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Set body */
    if (ftn_msg->text) {
        error = rfc822_message_set_body(msg, ftn_msg->text);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    *rfc_msg = msg;
    return FTN_OK;
    
error_cleanup:
    rfc822_message_free(msg);
    return error;
}

/* Convert RFC822 message to FTN */
ftn_error_t rfc822_to_ftn(const rfc822_message_t* rfc_msg, const char* domain, ftn_message_t** ftn_msg) {
    ftn_message_t* msg;
    const char* header_value;
    char* name = NULL;
    ftn_error_t error;
    unsigned int attributes;
    
    if (!rfc_msg || !domain || !ftn_msg) return FTN_ERROR_INVALID_PARAMETER;
    
    msg = ftn_message_new(FTN_MSG_NETMAIL);
    if (!msg) return FTN_ERROR_NOMEM;
    
    /* Extract From address */
    header_value = rfc822_message_get_header(rfc_msg, "From");
    if (header_value) {
        error = rfc822_address_to_ftn(header_value, domain, &msg->orig_addr, &name);
        if (error == FTN_OK && name) {
            msg->from_user = name;
            name = NULL;
        }
    }
    
    /* Extract To address */
    header_value = rfc822_message_get_header(rfc_msg, "To");
    if (header_value) {
        error = rfc822_address_to_ftn(header_value, domain, &msg->dest_addr, &name);
        if (error == FTN_OK && name) {
            msg->to_user = name;
            name = NULL;
        }
    }
    
    /* Extract Subject */
    header_value = rfc822_message_get_header(rfc_msg, "Subject");
    if (header_value) {
        msg->subject = malloc(strlen(header_value) + 1);
        if (msg->subject) {
            strcpy(msg->subject, header_value);
        }
    }
    
    /* Extract Date */
    header_value = rfc822_message_get_header(rfc_msg, "Date");
    if (header_value) {
        rfc822_date_to_timestamp(header_value, &msg->timestamp);
    }
    
    /* Extract Message-ID */
    header_value = rfc822_message_get_header(rfc_msg, "Message-ID");
    if (header_value) {
        msg->msgid = malloc(strlen(header_value) + 1);
        if (msg->msgid) {
            strcpy(msg->msgid, header_value);
        }
    }
    
    /* Extract In-Reply-To */
    header_value = rfc822_message_get_header(rfc_msg, "In-Reply-To");
    if (header_value) {
        msg->reply = malloc(strlen(header_value) + 1);
        if (msg->reply) {
            strcpy(msg->reply, header_value);
        }
    }
    
    /* Extract FTN attributes */
    header_value = rfc822_message_get_header(rfc_msg, "X-FTN-Attributes");
    if (header_value && sscanf(header_value, "0x%X", &attributes) == 1) {
        msg->attributes = attributes;
    }
    
    /* Extract area (for Echomail) */
    header_value = rfc822_message_get_header(rfc_msg, "X-FTN-Area");
    if (header_value) {
        msg->area = malloc(strlen(header_value) + 1);
        if (msg->area) {
            strcpy(msg->area, header_value);
            /* Change message type to Echomail if area is present */
            msg->type = FTN_MSG_ECHOMAIL;
        }
    }
    
    /* Extract origin line */
    header_value = rfc822_message_get_header(rfc_msg, "X-FTN-Origin");
    if (header_value) {
        msg->origin = malloc(strlen(header_value) + 1);
        if (msg->origin) {
            strcpy(msg->origin, header_value);
        }
    }
    
    /* Extract tearline */
    header_value = rfc822_message_get_header(rfc_msg, "X-FTN-Tearline");
    if (header_value) {
        msg->tearline = malloc(strlen(header_value) + 1);
        if (msg->tearline) {
            strcpy(msg->tearline, header_value);
        }
    }
    
    /* Set body */
    if (rfc_msg->body) {
        msg->text = malloc(strlen(rfc_msg->body) + 1);
        if (msg->text) {
            strcpy(msg->text, rfc_msg->body);
        }
    }
    
    *ftn_msg = msg;
    return FTN_OK;
}

/* Generate newsgroup name from network and area */
char* ftn_area_to_newsgroup(const char* network, const char* area) {
    char* newsgroup;
    char* lower_area;
    size_t len;
    size_t i;
    
    if (!network || !area) return NULL;
    
    /* Create lowercase copy of area name */
    len = strlen(area);
    lower_area = malloc(len + 1);
    if (!lower_area) return NULL;
    
    for (i = 0; i < len; i++) {
        lower_area[i] = tolower(area[i]);
    }
    lower_area[len] = '\0';
    
    /* Create newsgroup name: network.area */
    len = strlen(network) + 1 + strlen(lower_area) + 1;
    newsgroup = malloc(len);
    if (newsgroup) {
        snprintf(newsgroup, len, "%s.%s", network, lower_area);
    }
    
    free(lower_area);
    return newsgroup;
}

/* Extract area name from newsgroup */
char* newsgroup_to_ftn_area(const char* newsgroup, const char* network) {
    const char* dot_pos;
    char* area;
    size_t network_len;
    size_t area_len;
    size_t i;
    
    if (!newsgroup || !network) return NULL;
    
    network_len = strlen(network);
    
    /* Check if newsgroup starts with network. */
    if (strncmp(newsgroup, network, network_len) != 0 ||
        newsgroup[network_len] != '.') {
        return NULL;
    }
    
    /* Extract area part after the dot */
    dot_pos = newsgroup + network_len + 1;
    area_len = strlen(dot_pos);
    
    area = malloc(area_len + 1);
    if (!area) return NULL;
    
    /* Convert to uppercase for FTN convention */
    for (i = 0; i < area_len; i++) {
        area[i] = toupper(dot_pos[i]);
    }
    area[area_len] = '\0';
    
    return area;
}

/* Convert FTN Echomail message to RFC1036 USENET article */
ftn_error_t ftn_to_usenet(const ftn_message_t* ftn_msg, const char* network, rfc822_message_t** usenet_msg) {
    rfc822_message_t* msg;
    char* from_addr;
    char* newsgroup;
    char* date_str;
    char buffer[256];
    ftn_error_t error;
    size_t i;
    
    if (!ftn_msg || !network || !usenet_msg) return FTN_ERROR_INVALID_PARAMETER;
    
    /* Only convert Echomail messages */
    if (ftn_msg->type != FTN_MSG_ECHOMAIL || !ftn_msg->area) {
        return FTN_ERROR_INVALID_PARAMETER;
    }
    
    msg = rfc822_message_new();
    if (!msg) return FTN_ERROR_NOMEM;
    
    /* Set From header (use default domain for USENET) */
    from_addr = ftn_address_to_rfc822(&ftn_msg->orig_addr, ftn_msg->from_user, "fidonet.org");
    if (from_addr) {
        error = rfc822_message_add_header(msg, "From", from_addr);
        free(from_addr);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Set Newsgroups header */
    newsgroup = ftn_area_to_newsgroup(network, ftn_msg->area);
    if (newsgroup) {
        error = rfc822_message_add_header(msg, "Newsgroups", newsgroup);
        free(newsgroup);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Set Subject header */
    if (ftn_msg->subject) {
        error = rfc822_message_add_header(msg, "Subject", ftn_msg->subject);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Set Date header */
    date_str = ftn_timestamp_to_rfc822(ftn_msg->timestamp);
    if (date_str) {
        error = rfc822_message_add_header(msg, "Date", date_str);
        free(date_str);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Set Message-ID header if MSGID is available */
    if (ftn_msg->msgid) {
        error = rfc822_message_add_header(msg, "Message-ID", ftn_msg->msgid);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Set References header if REPLY is available */
    if (ftn_msg->reply) {
        error = rfc822_message_add_header(msg, "References", ftn_msg->reply);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Add Organization header (origin line) */
    if (ftn_msg->origin) {
        error = rfc822_message_add_header(msg, "Organization", ftn_msg->origin);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Add FTN-specific X- headers for round-trip compatibility */
    snprintf(buffer, sizeof(buffer), "%u:%u/%u.%u", ftn_msg->orig_addr.zone, ftn_msg->orig_addr.net,
             ftn_msg->orig_addr.node, ftn_msg->orig_addr.point);
    error = rfc822_message_add_header(msg, "X-FTN-From", buffer);
    if (error != FTN_OK) goto error_cleanup;
    
    /* Add X-FTN-Area header */
    if (ftn_msg->area) {
        error = rfc822_message_add_header(msg, "X-FTN-Area", ftn_msg->area);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    if (ftn_msg->attributes) {
        snprintf(buffer, sizeof(buffer), "0x%04X", ftn_msg->attributes);
        error = rfc822_message_add_header(msg, "X-FTN-Attributes", buffer);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Add control paragraphs as X-FTN headers */
    for (i = 0; i < ftn_msg->control_count; i++) {
        if (ftn_msg->control_lines[i]) {
            error = rfc822_message_add_header(msg, "X-FTN-Control", ftn_msg->control_lines[i]);
            if (error != FTN_OK) goto error_cleanup;
        }
    }
    
    /* Add SEEN-BY lines */
    for (i = 0; i < ftn_msg->seenby_count; i++) {
        if (ftn_msg->seenby[i]) {
            error = rfc822_message_add_header(msg, "X-FTN-Seen-By", ftn_msg->seenby[i]);
            if (error != FTN_OK) goto error_cleanup;
        }
    }
    
    /* Add PATH lines */
    for (i = 0; i < ftn_msg->path_count; i++) {
        if (ftn_msg->path[i]) {
            error = rfc822_message_add_header(msg, "X-FTN-Path", ftn_msg->path[i]);
            if (error != FTN_OK) goto error_cleanup;
        }
    }
    
    /* Add tearline */
    if (ftn_msg->tearline) {
        error = rfc822_message_add_header(msg, "X-FTN-Tearline", ftn_msg->tearline);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    /* Set body */
    if (ftn_msg->text) {
        error = rfc822_message_set_body(msg, ftn_msg->text);
        if (error != FTN_OK) goto error_cleanup;
    }
    
    *usenet_msg = msg;
    return FTN_OK;
    
error_cleanup:
    rfc822_message_free(msg);
    return error;
}

/* Convert RFC1036 USENET article to FTN Echomail message */
ftn_error_t usenet_to_ftn(const rfc822_message_t* usenet_msg, const char* network, ftn_message_t** ftn_msg) {
    ftn_message_t* msg;
    const char* header_value;
    char* name = NULL;
    char* area;
    ftn_error_t error;
    unsigned int attributes;
    
    if (!usenet_msg || !network || !ftn_msg) return FTN_ERROR_INVALID_PARAMETER;
    
    msg = ftn_message_new(FTN_MSG_ECHOMAIL);
    if (!msg) return FTN_ERROR_NOMEM;
    
    /* Extract area from Newsgroups header */
    header_value = rfc822_message_get_header(usenet_msg, "Newsgroups");
    if (header_value) {
        area = newsgroup_to_ftn_area(header_value, network);
        if (area) {
            msg->area = area;
        }
    }
    
    /* Extract From user name from regular From header */
    header_value = rfc822_message_get_header(usenet_msg, "From");
    if (header_value) {
        /* Try FTN-style address first */
        ftn_address_t temp_addr;
        error = rfc822_address_to_ftn(header_value, "fidonet.org", &temp_addr, &name);
        if (error == FTN_OK && name) {
            msg->from_user = name;
            name = NULL;
        } else {
            /* For regular email addresses, extract name from "Name <email>" format */
            const char* start = header_value;
            const char* bracket = strchr(header_value, '<');
            const char* end;
            if (bracket && bracket > start) {
                /* Skip quotes if present */
                if (*start == '"') start++;
                end = bracket - 1;
                /* Skip trailing quotes and whitespace */
                while (end > start && (*end == ' ' || *end == '"')) end--;
                if (end > start) {
                    size_t name_len = end - start + 1;
                    msg->from_user = malloc(name_len + 1);
                    if (msg->from_user) {
                        strncpy(msg->from_user, start, name_len);
                        msg->from_user[name_len] = '\0';
                    }
                }
            } else if (!bracket) {
                /* Just the email address, use the part before @ as name */
                const char* at = strchr(header_value, '@');
                if (at && at > start) {
                    size_t name_len = at - start;
                    msg->from_user = malloc(name_len + 1);
                    if (msg->from_user) {
                        strncpy(msg->from_user, start, name_len);
                        msg->from_user[name_len] = '\0';
                    }
                }
            }
        }
    }
    
    /* Use X-FTN-From for the actual FTN origin address */
    header_value = rfc822_message_get_header(usenet_msg, "X-FTN-From");
    if (header_value) {
        ftn_address_parse(header_value, &msg->orig_addr);
    }
    
    /* For Echomail, we need a destination address - use a default uplink if not specified */
    /* This could be enhanced to extract from routing headers or use configuration */
    msg->dest_addr.zone = msg->orig_addr.zone;
    msg->dest_addr.net = msg->orig_addr.net;
    msg->dest_addr.node = 1; /* Default to net coordinator */
    msg->dest_addr.point = 0;
    
    /* Set To user as "All" for Echomail */
    msg->to_user = malloc(4);
    if (msg->to_user) {
        strcpy(msg->to_user, "All");
    }
    
    /* Extract Subject */
    header_value = rfc822_message_get_header(usenet_msg, "Subject");
    if (header_value) {
        msg->subject = malloc(strlen(header_value) + 1);
        if (msg->subject) {
            strcpy(msg->subject, header_value);
        }
    }
    
    /* Extract Date */
    header_value = rfc822_message_get_header(usenet_msg, "Date");
    if (header_value) {
        rfc822_date_to_timestamp(header_value, &msg->timestamp);
    }
    
    /* Extract Message-ID */
    header_value = rfc822_message_get_header(usenet_msg, "Message-ID");
    if (header_value) {
        msg->msgid = malloc(strlen(header_value) + 1);
        if (msg->msgid) {
            strcpy(msg->msgid, header_value);
        }
    }
    
    /* Extract References (use as REPLY) */
    header_value = rfc822_message_get_header(usenet_msg, "References");
    if (header_value) {
        msg->reply = malloc(strlen(header_value) + 1);
        if (msg->reply) {
            strcpy(msg->reply, header_value);
        }
    }
    
    /* Extract Organization (use as origin) */
    header_value = rfc822_message_get_header(usenet_msg, "Organization");
    if (header_value) {
        msg->origin = malloc(strlen(header_value) + 1);
        if (msg->origin) {
            strcpy(msg->origin, header_value);
        }
    }
    
    /* Extract FTN attributes */
    header_value = rfc822_message_get_header(usenet_msg, "X-FTN-Attributes");
    if (header_value && sscanf(header_value, "0x%X", &attributes) == 1) {
        msg->attributes = attributes;
    }
    
    /* Extract tearline */
    header_value = rfc822_message_get_header(usenet_msg, "X-FTN-Tearline");
    if (header_value) {
        msg->tearline = malloc(strlen(header_value) + 1);
        if (msg->tearline) {
            strcpy(msg->tearline, header_value);
        }
    }
    
    /* Set body */
    if (usenet_msg->body) {
        msg->text = malloc(strlen(usenet_msg->body) + 1);
        if (msg->text) {
            strcpy(msg->text, usenet_msg->body);
        }
    }
    
    *ftn_msg = msg;
    return FTN_OK;
}