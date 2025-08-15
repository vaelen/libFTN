/*
 * libFTN - FidoNet (FTN) Packet and Message Implementation
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "../include/ftn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Helper function to read a 16-bit little-endian integer */
static unsigned int read_uint16(FILE* fp) {
    unsigned char bytes[2];
    if (fread(bytes, 1, 2, fp) != 2) {
        return 0;
    }
    return bytes[0] | (bytes[1] << 8);
}

/* Helper function to write a 16-bit little-endian integer */
static int write_uint16(FILE* fp, unsigned int value) {
    unsigned char bytes[2];
    bytes[0] = value & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    return fwrite(bytes, 1, 2, fp) == 2 ? 1 : 0;
}

/* Helper function to read a packed string */
static char* read_packed_string(FILE* fp, size_t max_len) {
    char* buffer;
    size_t len = 0;
    int c;
    
    buffer = malloc(max_len + 1);
    if (!buffer) return NULL;
    
    while (len < max_len && (c = fgetc(fp)) != EOF && c != 0) {
        buffer[len++] = c;
    }
    buffer[len] = '\0';
    
    return buffer;
}

/* Helper function to write a packed string */
static int write_packed_string(FILE* fp, const char* str, size_t max_len) {
    size_t len;
    
    if (!str) str = "";
    len = strlen(str);
    if (len > max_len) len = max_len;
    
    if (fwrite(str, 1, len, fp) != len) return 0;
    
    /* Write null terminator */
    if (fputc(0, fp) == EOF) return 0;
    
    return 1;
}

ftn_packet_t* ftn_packet_new(void) {
    ftn_packet_t* packet;
    
    packet = malloc(sizeof(ftn_packet_t));
    if (!packet) return NULL;
    
    memset(packet, 0, sizeof(ftn_packet_t));
    packet->message_capacity = 4;
    packet->messages = malloc(packet->message_capacity * sizeof(ftn_message_t*));
    if (!packet->messages) {
        free(packet);
        return NULL;
    }
    
    return packet;
}

void ftn_packet_free(ftn_packet_t* packet) {
    size_t i;
    
    if (!packet) return;
    
    if (packet->messages) {
        for (i = 0; i < packet->message_count; i++) {
            ftn_message_free(packet->messages[i]);
        }
        free(packet->messages);
    }
    
    free(packet);
}

ftn_error_t ftn_packet_load(const char* filename, ftn_packet_t** packet) {
    FILE* fp;
    ftn_packet_t* pkt;
    ftn_packet_header_t* header;
    unsigned int msg_type;
    ftn_message_t* message;
    ftn_packed_msg_header_t msg_header;
    
    if (!filename || !packet) return FTN_ERROR_INVALID_PARAMETER;
    
    *packet = NULL;
    
    fp = fopen(filename, "rb");
    if (!fp) return FTN_ERROR_FILE_NOT_FOUND;
    
    pkt = ftn_packet_new();
    if (!pkt) {
        fclose(fp);
        return FTN_ERROR_MEMORY;
    }
    
    header = &pkt->header;
    
    /* Read packet header (58 bytes) */
    header->orig_node = read_uint16(fp);
    header->dest_node = read_uint16(fp);
    header->year = read_uint16(fp);
    header->month = read_uint16(fp);
    header->day = read_uint16(fp);
    header->hour = read_uint16(fp);
    header->minute = read_uint16(fp);
    header->second = read_uint16(fp);
    header->baud = read_uint16(fp);
    header->packet_type = read_uint16(fp);
    header->orig_net = read_uint16(fp);
    header->dest_net = read_uint16(fp);
    
    /* Read product code and serial number */
    if (fread(&header->prod_code, 1, 1, fp) != 1 ||
        fread(&header->serial_no, 1, 1, fp) != 1) {
        ftn_packet_free(pkt);
        fclose(fp);
        return FTN_ERROR_INVALID_FORMAT;
    }
    
    /* Read password (8 bytes) */
    if (fread(header->password, 1, 8, fp) != 8) {
        ftn_packet_free(pkt);
        fclose(fp);
        return FTN_ERROR_INVALID_FORMAT;
    }
    
    /* Read optional zone fields */
    header->orig_zone = read_uint16(fp);
    header->dest_zone = read_uint16(fp);
    
    /* Read fill bytes */
    if (fread(header->fill, 1, 20, fp) != 20) {
        ftn_packet_free(pkt);
        fclose(fp);
        return FTN_ERROR_INVALID_FORMAT;
    }
    
    /* Read messages */
    while ((msg_type = read_uint16(fp)) != 0) {
        if (msg_type != 0x0002) {
            /* Invalid message type */
            ftn_packet_free(pkt);
            fclose(fp);
            return FTN_ERROR_INVALID_FORMAT;
        }
        
        /* Read packed message header */
        msg_header.message_type = msg_type;
        msg_header.orig_node = read_uint16(fp);
        msg_header.dest_node = read_uint16(fp);
        msg_header.orig_net = read_uint16(fp);
        msg_header.dest_net = read_uint16(fp);
        msg_header.attributes = read_uint16(fp);
        msg_header.cost = read_uint16(fp);
        
        /* Read datetime string (20 bytes) */
        if (fread(msg_header.datetime, 1, 20, fp) != 20) {
            ftn_packet_free(pkt);
            fclose(fp);
            return FTN_ERROR_INVALID_FORMAT;
        }
        
        /* Create message */
        message = ftn_message_new(FTN_MSG_NETMAIL);
        if (!message) {
            ftn_packet_free(pkt);
            fclose(fp);
            return FTN_ERROR_MEMORY;
        }
        
        /* Fill in message data */
        message->orig_addr.zone = header->orig_zone;
        message->orig_addr.net = msg_header.orig_net;
        message->orig_addr.node = msg_header.orig_node;
        message->orig_addr.point = 0;
        
        message->dest_addr.zone = header->dest_zone;
        message->dest_addr.net = msg_header.dest_net;
        message->dest_addr.node = msg_header.dest_node;
        message->dest_addr.point = 0;
        
        message->attributes = msg_header.attributes;
        message->cost = msg_header.cost;
        
        /* Parse datetime */
        ftn_datetime_from_string(msg_header.datetime, &message->timestamp);
        
        /* Read variable-length strings */
        message->to_user = read_packed_string(fp, 35);
        message->from_user = read_packed_string(fp, 35);
        message->subject = read_packed_string(fp, 71);
        message->text = read_packed_string(fp, 65535);
        
        if (!message->to_user || !message->from_user || 
            !message->subject || !message->text) {
            ftn_message_free(message);
            ftn_packet_free(pkt);
            fclose(fp);
            return FTN_ERROR_MEMORY;
        }
        
        /* Parse message text for control information */
        ftn_message_parse_text(message, message->text);
        
        /* Add message to packet */
        if (ftn_packet_add_message(pkt, message) != FTN_OK) {
            ftn_message_free(message);
            ftn_packet_free(pkt);
            fclose(fp);
            return FTN_ERROR_MEMORY;
        }
    }
    
    fclose(fp);
    *packet = pkt;
    return FTN_OK;
}

ftn_error_t ftn_packet_save(const char* filename, const ftn_packet_t* packet) {
    FILE* fp;
    const ftn_packet_header_t* header;
    size_t i;
    ftn_message_t* message;
    char datetime_str[21];
    char* full_text;
    
    if (!filename || !packet) return FTN_ERROR_INVALID_PARAMETER;
    
    fp = fopen(filename, "wb");
    if (!fp) return FTN_ERROR_FILE_ACCESS;
    
    header = &packet->header;
    
    /* Write packet header */
    if (!write_uint16(fp, header->orig_node) ||
        !write_uint16(fp, header->dest_node) ||
        !write_uint16(fp, header->year) ||
        !write_uint16(fp, header->month) ||
        !write_uint16(fp, header->day) ||
        !write_uint16(fp, header->hour) ||
        !write_uint16(fp, header->minute) ||
        !write_uint16(fp, header->second) ||
        !write_uint16(fp, header->baud) ||
        !write_uint16(fp, header->packet_type) ||
        !write_uint16(fp, header->orig_net) ||
        !write_uint16(fp, header->dest_net)) {
        fclose(fp);
        return FTN_ERROR_FILE_ACCESS;
    }
    
    /* Write product code and serial number */
    if (fwrite(&header->prod_code, 1, 1, fp) != 1 ||
        fwrite(&header->serial_no, 1, 1, fp) != 1) {
        fclose(fp);
        return FTN_ERROR_FILE_ACCESS;
    }
    
    /* Write password and zones */
    if (fwrite(header->password, 1, 8, fp) != 8 ||
        !write_uint16(fp, header->orig_zone) ||
        !write_uint16(fp, header->dest_zone) ||
        fwrite(header->fill, 1, 20, fp) != 20) {
        fclose(fp);
        return FTN_ERROR_FILE_ACCESS;
    }
    
    /* Write messages */
    for (i = 0; i < packet->message_count; i++) {
        message = packet->messages[i];
        
        /* Write message type */
        if (!write_uint16(fp, 0x0002)) {
            fclose(fp);
            return FTN_ERROR_FILE_ACCESS;
        }
        
        /* Write packed message header */
        if (!write_uint16(fp, message->orig_addr.node) ||
            !write_uint16(fp, message->dest_addr.node) ||
            !write_uint16(fp, message->orig_addr.net) ||
            !write_uint16(fp, message->dest_addr.net) ||
            !write_uint16(fp, message->attributes) ||
            !write_uint16(fp, message->cost)) {
            fclose(fp);
            return FTN_ERROR_FILE_ACCESS;
        }
        
        /* Write datetime */
        ftn_datetime_to_string(message->timestamp, datetime_str, sizeof(datetime_str));
        if (fwrite(datetime_str, 1, 20, fp) != 20) {
            fclose(fp);
            return FTN_ERROR_FILE_ACCESS;
        }
        
        /* Write strings */
        if (!write_packed_string(fp, message->to_user, 35) ||
            !write_packed_string(fp, message->from_user, 35) ||
            !write_packed_string(fp, message->subject, 71)) {
            fclose(fp);
            return FTN_ERROR_FILE_ACCESS;
        }
        
        /* Create full message text with control lines */
        full_text = ftn_message_create_text(message);
        if (!full_text) {
            fclose(fp);
            return FTN_ERROR_MEMORY;
        }
        
        if (!write_packed_string(fp, full_text, 65535)) {
            free(full_text);
            fclose(fp);
            return FTN_ERROR_FILE_ACCESS;
        }
        
        free(full_text);
    }
    
    /* Write packet terminator */
    if (!write_uint16(fp, 0x0000)) {
        fclose(fp);
        return FTN_ERROR_FILE_ACCESS;
    }
    
    fclose(fp);
    return FTN_OK;
}

ftn_error_t ftn_packet_add_message(ftn_packet_t* packet, ftn_message_t* message) {
    ftn_message_t** temp;
    
    if (!packet || !message) return FTN_ERROR_INVALID_PARAMETER;
    
    /* Resize array if needed */
    if (packet->message_count >= packet->message_capacity) {
        packet->message_capacity *= 2;
        temp = realloc(packet->messages, 
                      packet->message_capacity * sizeof(ftn_message_t*));
        if (!temp) return FTN_ERROR_MEMORY;
        packet->messages = temp;
    }
    
    packet->messages[packet->message_count++] = message;
    return FTN_OK;
}

ftn_message_t* ftn_message_new(ftn_message_type_t type) {
    ftn_message_t* message;
    
    message = malloc(sizeof(ftn_message_t));
    if (!message) return NULL;
    
    memset(message, 0, sizeof(ftn_message_t));
    message->type = type;
    time(&message->timestamp);
    
    return message;
}

void ftn_message_free(ftn_message_t* message) {
    size_t i;
    
    if (!message) return;
    
    if (message->to_user) free(message->to_user);
    if (message->from_user) free(message->from_user);
    if (message->subject) free(message->subject);
    if (message->text) free(message->text);
    if (message->area) free(message->area);
    if (message->origin) free(message->origin);
    if (message->tearline) free(message->tearline);
    if (message->msgid) free(message->msgid);
    if (message->reply) free(message->reply);
    
    if (message->seenby) {
        for (i = 0; i < message->seenby_count; i++) {
            if (message->seenby[i]) free(message->seenby[i]);
        }
        free(message->seenby);
    }
    
    if (message->path) {
        for (i = 0; i < message->path_count; i++) {
            if (message->path[i]) free(message->path[i]);
        }
        free(message->path);
    }
    
    free(message);
}

/* Date/time conversion functions */
ftn_error_t ftn_datetime_to_string(time_t timestamp, char* buffer, size_t size) {
    struct tm* tm_info;
    const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    
    if (!buffer || size < 21) return FTN_ERROR_INVALID_PARAMETER;
    
    tm_info = localtime(&timestamp);
    if (!tm_info) return FTN_ERROR_INVALID_PARAMETER;
    
    /* Format: "01 Jan 86  02:34:56\0" (20 chars + null) */
    snprintf(buffer, size, "%02d %s %02d  %02d:%02d:%02d",
             tm_info->tm_mday,
             months[tm_info->tm_mon],
             tm_info->tm_year % 100,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);
    
    /* Ensure exactly 20 characters */
    buffer[20] = '\0';
    
    return FTN_OK;
}

ftn_error_t ftn_datetime_from_string(const char* datetime_str, time_t* timestamp) {
    struct tm tm_info;
    char month_str[4];
    int day, year, hour, min, sec;
    int month;
    const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    
    if (!datetime_str || !timestamp) return FTN_ERROR_INVALID_PARAMETER;
    
    /* Parse: "01 Jan 86  02:34:56" */
    if (sscanf(datetime_str, "%d %3s %d %d:%d:%d", 
               &day, month_str, &year, &hour, &min, &sec) != 6) {
        return FTN_ERROR_INVALID_FORMAT;
    }
    
    /* Find month */
    month = -1;
    for (month = 0; month < 12; month++) {
        if (strcmp(month_str, months[month]) == 0) break;
    }
    if (month >= 12) return FTN_ERROR_INVALID_FORMAT;
    
    /* Fill tm structure */
    memset(&tm_info, 0, sizeof(tm_info));
    tm_info.tm_mday = day;
    tm_info.tm_mon = month;
    tm_info.tm_year = (year < 80) ? year + 100 : year;  /* Y2K handling */
    tm_info.tm_hour = hour;
    tm_info.tm_min = min;
    tm_info.tm_sec = sec;
    tm_info.tm_isdst = -1;
    
    *timestamp = mktime(&tm_info);
    if (*timestamp == -1) return FTN_ERROR_INVALID_FORMAT;
    return FTN_OK;
}

/* Utility functions */
int ftn_message_has_attribute(const ftn_message_t* message, unsigned int attr) {
    if (!message) return 0;
    return (message->attributes & attr) != 0;
}

void ftn_message_set_attribute(ftn_message_t* message, unsigned int attr) {
    if (!message) return;
    message->attributes |= attr;
}

void ftn_message_clear_attribute(ftn_message_t* message, unsigned int attr) {
    if (!message) return;
    message->attributes &= ~attr;
}

ftn_error_t ftn_message_parse_text(ftn_message_t* message, const char* text) {
    char* work_text;
    char* line;
    char* saveptr;
    char* body_lines[1000];  /* Maximum body lines */
    int body_line_count = 0;
    int in_body = 0;
    
    if (!message || !text) return FTN_ERROR_INVALID_PARAMETER;
    
    work_text = strdup(text);
    if (!work_text) {
        return FTN_ERROR_MEMORY;
    }
    
    /* Parse all lines - control lines can appear anywhere, don't stop body collection */
    line = strtok_r(work_text, "\r", &saveptr);
    while (line) {
        char* trimmed_line = strdup(line);
        if (!trimmed_line) {
            free(work_text);
            return FTN_ERROR_MEMORY;
        }
        ftn_trim(trimmed_line);
        
        /* Check for AREA line (first line for echomail) */
        if (strncmp(trimmed_line, "AREA:", 5) == 0) {
            message->type = FTN_MSG_ECHOMAIL;
            if (message->area) free(message->area);
            message->area = strdup(trimmed_line + 5);
            ftn_trim(message->area);
            /* AREA line is not part of message body */
        }
        /* Check for control-A lines */
        else if (trimmed_line[0] == '\001') {
            if (strncmp(trimmed_line, "\001MSGID:", 7) == 0) {
                if (message->msgid) free(message->msgid);
                message->msgid = strdup(trimmed_line + 7);
                ftn_trim(message->msgid);
            }
            else if (strncmp(trimmed_line, "\001REPLY:", 7) == 0) {
                if (message->reply) free(message->reply);
                message->reply = strdup(trimmed_line + 7);
                ftn_trim(message->reply);
            }
            else if (strncmp(trimmed_line, "\001PATH:", 6) == 0) {
                ftn_message_add_path(message, trimmed_line + 6);
            }
            /* Control-A lines are not part of message body */
        }
        /* Check for tear line - store it but continue parsing body */
        else if (strncmp(trimmed_line, "--- ", 4) == 0 && strlen(trimmed_line) > 4 && trimmed_line[4] != '-') {
            if (message->tearline) free(message->tearline);
            message->tearline = strdup(trimmed_line);
            /* Tearline can appear anywhere - don't stop body collection */
        }
        /* Check for origin line - typically signals end but don't stop parsing */
        else if (strncmp(trimmed_line, "* Origin:", 9) == 0) {
            if (message->origin) free(message->origin);
            message->origin = strdup(trimmed_line);
            /* Origin line traditionally marks end, but can appear anywhere */
        }
        /* Check for SEEN-BY lines */
        else if (strncmp(trimmed_line, "SEEN-BY:", 8) == 0) {
            ftn_message_add_seenby(message, trimmed_line + 8);
            /* SEEN-BY lines are routing info, not message body */
        }
        /* Check for PATH lines */
        else if (strncmp(trimmed_line, "PATH:", 5) == 0) {
            ftn_message_add_path(message, trimmed_line + 5);
            /* PATH lines are routing info, not message body */
        }
        /* Everything else is potentially message body content */
        else {
            /* Start collecting body content after initial control lines */
            if (!in_body) {
                in_body = 1;
            }
            
            /* Include all non-control lines in body, including empty lines */
            if (body_line_count < 999) {
                body_lines[body_line_count] = strdup(line);  /* Keep original spacing */
                if (body_lines[body_line_count]) {
                    body_line_count++;
                }
            }
        }
        
        free(trimmed_line);
        line = strtok_r(NULL, "\r", &saveptr);
    }
    
    /* Reconstruct clean message body */
    if (body_line_count > 0) {
        size_t total_len = 0;
        int i;
        
        /* Calculate total length needed */
        for (i = 0; i < body_line_count; i++) {
            total_len += strlen(body_lines[i]);
            if (i < body_line_count - 1) {
                total_len += 2;  /* +2 for \r\n */
            }
        }
        
        if (message->text) free(message->text);
        message->text = malloc(total_len + 1);
        if (message->text) {
            message->text[0] = '\0';
            for (i = 0; i < body_line_count; i++) {
                strcat(message->text, body_lines[i]);
                if (i < body_line_count - 1) {
                    strcat(message->text, "\r\n");
                }
            }
            ftn_trim(message->text);
        }
        
        /* Free body lines */
        for (i = 0; i < body_line_count; i++) {
            free(body_lines[i]);
        }
    } else {
        /* No body content */
        if (message->text) free(message->text);
        message->text = strdup("");
    }
    
    free(work_text);
    return FTN_OK;
}

char* ftn_message_create_text(const ftn_message_t* message) {
    char* result;
    size_t total_len = 0;
    size_t i;
    
    if (!message) return NULL;
    
    /* Calculate total length needed */
    if (message->area) {
        total_len += strlen("AREA:") + strlen(message->area) + 2;
    }
    if (message->msgid) {
        total_len += 8 + strlen(message->msgid) + 2;  /* \001MSGID: */
    }
    if (message->reply) {
        total_len += 8 + strlen(message->reply) + 2;  /* \001REPLY: */
    }
    if (message->text) {
        total_len += strlen(message->text) + 2;
    }
    if (message->tearline) {
        total_len += strlen(message->tearline) + 2;
    }
    if (message->origin) {
        total_len += strlen(message->origin) + 2;
    }
    for (i = 0; i < message->seenby_count; i++) {
        if (message->seenby[i]) {
            total_len += strlen("SEEN-BY:") + strlen(message->seenby[i]) + 2;
        }
    }
    for (i = 0; i < message->path_count; i++) {
        if (message->path[i]) {
            total_len += 7 + strlen(message->path[i]) + 2;  /* \001PATH: */
        }
    }
    
    total_len += 1;  /* null terminator */
    
    result = malloc(total_len);
    if (!result) return NULL;
    result[0] = '\0';
    
    /* Build the message text */
    if (message->area) {
        strcat(result, "AREA:");
        strcat(result, message->area);
        strcat(result, "\r\n");
    }
    
    if (message->msgid) {
        strcat(result, "\001MSGID: ");
        strcat(result, message->msgid);
        strcat(result, "\r\n");
    }
    
    if (message->reply) {
        strcat(result, "\001REPLY: ");
        strcat(result, message->reply);
        strcat(result, "\r\n");
    }
    
    if (message->text) {
        strcat(result, message->text);
        strcat(result, "\r\n");
    }
    
    if (message->tearline) {
        strcat(result, message->tearline);
        strcat(result, "\r\n");
    }
    
    if (message->origin) {
        strcat(result, message->origin);
        strcat(result, "\r\n");
    }
    
    for (i = 0; i < message->seenby_count; i++) {
        if (message->seenby[i]) {
            strcat(result, "SEEN-BY:");
            strcat(result, message->seenby[i]);
            strcat(result, "\r\n");
        }
    }
    
    for (i = 0; i < message->path_count; i++) {
        if (message->path[i]) {
            strcat(result, "\001PATH: ");
            strcat(result, message->path[i]);
            strcat(result, "\r\n");
        }
    }
    
    return result;
}

ftn_error_t ftn_message_add_seenby(ftn_message_t* message, const char* seenby) {
    char** temp;
    
    if (!message || !seenby) return FTN_ERROR_INVALID_PARAMETER;
    
    temp = realloc(message->seenby, (message->seenby_count + 1) * sizeof(char*));
    if (!temp) return FTN_ERROR_MEMORY;
    
    message->seenby = temp;
    message->seenby[message->seenby_count] = strdup(seenby);
    if (!message->seenby[message->seenby_count]) return FTN_ERROR_MEMORY;
    
    ftn_trim(message->seenby[message->seenby_count]);
    message->seenby_count++;
    
    return FTN_OK;
}

ftn_error_t ftn_message_add_path(ftn_message_t* message, const char* path) {
    char** temp;
    
    if (!message || !path) return FTN_ERROR_INVALID_PARAMETER;
    
    temp = realloc(message->path, (message->path_count + 1) * sizeof(char*));
    if (!temp) return FTN_ERROR_MEMORY;
    
    message->path = temp;
    message->path[message->path_count] = strdup(path);
    if (!message->path[message->path_count]) return FTN_ERROR_MEMORY;
    
    ftn_trim(message->path[message->path_count]);
    message->path_count++;
    
    return FTN_OK;
}

ftn_error_t ftn_message_set_msgid(ftn_message_t* message, const ftn_address_t* addr, const char* serial) {
    char addr_str[32];
    char* msgid_str;
    
    if (!message || !addr || !serial) return FTN_ERROR_INVALID_PARAMETER;
    
    ftn_address_to_string(addr, addr_str, sizeof(addr_str));
    
    msgid_str = malloc(strlen(addr_str) + strlen(serial) + 2);
    if (!msgid_str) return FTN_ERROR_MEMORY;
    
    sprintf(msgid_str, "%s %s", addr_str, serial);
    
    if (message->msgid) free(message->msgid);
    message->msgid = msgid_str;
    
    return FTN_OK;
}

ftn_error_t ftn_message_set_reply(ftn_message_t* message, const char* reply_msgid) {
    if (!message || !reply_msgid) return FTN_ERROR_INVALID_PARAMETER;
    
    if (message->reply) free(message->reply);
    message->reply = strdup(reply_msgid);
    
    return message->reply ? FTN_OK : FTN_ERROR_MEMORY;
}
