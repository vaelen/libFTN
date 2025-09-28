#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "ftn/binkp_commands.h"
#include "ftn/log.h"

ftn_binkp_error_t ftn_binkp_command_init(ftn_binkp_command_frame_t* cmd_frame) {
    if (!cmd_frame) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    memset(cmd_frame, 0, sizeof(ftn_binkp_command_frame_t));
    return BINKP_OK;
}

void ftn_binkp_command_free(ftn_binkp_command_frame_t* cmd_frame) {
    if (cmd_frame && cmd_frame->args) {
        free(cmd_frame->args);
        cmd_frame->args = NULL;
        cmd_frame->args_len = 0;
    }
}

ftn_binkp_error_t ftn_binkp_command_parse(const ftn_binkp_frame_t* frame, ftn_binkp_command_frame_t* cmd_frame) {
    if (!frame || !cmd_frame || !frame->is_command) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    if (frame->size == 0) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    ftn_binkp_command_free(cmd_frame);

    /* First byte is command type */
    cmd_frame->cmd = (ftn_binkp_command_t)frame->data[0];

    /* Rest is arguments */
    if (frame->size > 1) {
        cmd_frame->args_len = frame->size - 1;
        cmd_frame->args = malloc(cmd_frame->args_len + 1);
        if (!cmd_frame->args) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(cmd_frame->args, frame->data + 1, cmd_frame->args_len);
        cmd_frame->args[cmd_frame->args_len] = '\0';
    } else {
        cmd_frame->args = NULL;
        cmd_frame->args_len = 0;
    }

    ftn_log_debug("Parsed binkp command %s with %zu byte args",
                  ftn_binkp_command_name(cmd_frame->cmd), cmd_frame->args_len);
    return BINKP_OK;
}

ftn_binkp_error_t ftn_binkp_command_create(ftn_binkp_command_frame_t* cmd_frame, ftn_binkp_command_t cmd, const char* args) {
    size_t args_len;

    if (!cmd_frame) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    ftn_binkp_command_free(cmd_frame);

    cmd_frame->cmd = cmd;
    args_len = args ? strlen(args) : 0;

    if (args_len > 0) {
        cmd_frame->args = malloc(args_len + 1);
        if (!cmd_frame->args) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }
        strcpy(cmd_frame->args, args);
        cmd_frame->args_len = args_len;
    } else {
        cmd_frame->args = NULL;
        cmd_frame->args_len = 0;
    }

    return BINKP_OK;
}

ftn_binkp_error_t ftn_binkp_command_to_frame(const ftn_binkp_command_frame_t* cmd_frame, ftn_binkp_frame_t* frame) {
    uint8_t* buffer;
    size_t total_size;

    if (!cmd_frame || !frame) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    total_size = 1 + cmd_frame->args_len;
    buffer = malloc(total_size);
    if (!buffer) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    buffer[0] = (uint8_t)cmd_frame->cmd;
    if (cmd_frame->args_len > 0 && cmd_frame->args) {
        memcpy(buffer + 1, cmd_frame->args, cmd_frame->args_len);
    }

    {
        ftn_binkp_error_t result = ftn_binkp_frame_create(frame, 1, buffer, total_size);
        free(buffer);
        return result;
    }
}

ftn_binkp_error_t ftn_binkp_create_m_nul(ftn_binkp_frame_t* frame, const char* info) {
    ftn_binkp_command_frame_t cmd_frame;
    ftn_binkp_error_t result;

    ftn_binkp_command_init(&cmd_frame);
    result = ftn_binkp_command_create(&cmd_frame, BINKP_M_NUL, info);
    if (result == BINKP_OK) {
        result = ftn_binkp_command_to_frame(&cmd_frame, frame);
    }

    ftn_binkp_command_free(&cmd_frame);
    return result;
}

ftn_binkp_error_t ftn_binkp_create_m_adr(ftn_binkp_frame_t* frame, const char* addresses) {
    ftn_binkp_command_frame_t cmd_frame;
    ftn_binkp_error_t result;

    ftn_binkp_command_init(&cmd_frame);
    result = ftn_binkp_command_create(&cmd_frame, BINKP_M_ADR, addresses);
    if (result == BINKP_OK) {
        result = ftn_binkp_command_to_frame(&cmd_frame, frame);
    }

    ftn_binkp_command_free(&cmd_frame);
    return result;
}

ftn_binkp_error_t ftn_binkp_create_m_pwd(ftn_binkp_frame_t* frame, const char* password) {
    ftn_binkp_command_frame_t cmd_frame;
    ftn_binkp_error_t result;

    ftn_binkp_command_init(&cmd_frame);
    result = ftn_binkp_command_create(&cmd_frame, BINKP_M_PWD, password);
    if (result == BINKP_OK) {
        result = ftn_binkp_command_to_frame(&cmd_frame, frame);
    }

    ftn_binkp_command_free(&cmd_frame);
    return result;
}

ftn_binkp_error_t ftn_binkp_create_m_file(ftn_binkp_frame_t* frame, const ftn_binkp_file_info_t* file_info) {
    char* escaped_filename;
    char args_buffer[1024];
    ftn_binkp_command_frame_t cmd_frame;
    ftn_binkp_error_t result;

    if (!file_info || !file_info->filename) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    result = ftn_binkp_escape_filename(file_info->filename, &escaped_filename);
    if (result != BINKP_OK) {
        return result;
    }

    snprintf(args_buffer, sizeof(args_buffer), "%s %lu %lu %lu",
             escaped_filename,
             (unsigned long)file_info->file_size,
             (unsigned long)file_info->timestamp,
             (unsigned long)file_info->offset);

    ftn_binkp_command_init(&cmd_frame);
    result = ftn_binkp_command_create(&cmd_frame, BINKP_M_FILE, args_buffer);
    if (result == BINKP_OK) {
        result = ftn_binkp_command_to_frame(&cmd_frame, frame);
    }

    ftn_binkp_command_free(&cmd_frame);
    free(escaped_filename);
    return result;
}

ftn_binkp_error_t ftn_binkp_create_m_ok(ftn_binkp_frame_t* frame, const char* info) {
    ftn_binkp_command_frame_t cmd_frame;
    ftn_binkp_error_t result;

    ftn_binkp_command_init(&cmd_frame);
    result = ftn_binkp_command_create(&cmd_frame, BINKP_M_OK, info);
    if (result == BINKP_OK) {
        result = ftn_binkp_command_to_frame(&cmd_frame, frame);
    }

    ftn_binkp_command_free(&cmd_frame);
    return result;
}

ftn_binkp_error_t ftn_binkp_create_m_eob(ftn_binkp_frame_t* frame) {
    ftn_binkp_command_frame_t cmd_frame;
    ftn_binkp_error_t result;

    ftn_binkp_command_init(&cmd_frame);
    result = ftn_binkp_command_create(&cmd_frame, BINKP_M_EOB, NULL);
    if (result == BINKP_OK) {
        result = ftn_binkp_command_to_frame(&cmd_frame, frame);
    }

    ftn_binkp_command_free(&cmd_frame);
    return result;
}

ftn_binkp_error_t ftn_binkp_create_m_got(ftn_binkp_frame_t* frame, const char* filename, size_t bytes_received) {
    char* escaped_filename;
    char args_buffer[512];
    ftn_binkp_command_frame_t cmd_frame;
    ftn_binkp_error_t result;

    if (!filename) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    result = ftn_binkp_escape_filename(filename, &escaped_filename);
    if (result != BINKP_OK) {
        return result;
    }

    snprintf(args_buffer, sizeof(args_buffer), "%s %lu", escaped_filename, (unsigned long)bytes_received);

    ftn_binkp_command_init(&cmd_frame);
    result = ftn_binkp_command_create(&cmd_frame, BINKP_M_GOT, args_buffer);
    if (result == BINKP_OK) {
        result = ftn_binkp_command_to_frame(&cmd_frame, frame);
    }

    ftn_binkp_command_free(&cmd_frame);
    free(escaped_filename);
    return result;
}

ftn_binkp_error_t ftn_binkp_create_m_err(ftn_binkp_frame_t* frame, const char* error_msg) {
    ftn_binkp_command_frame_t cmd_frame;
    ftn_binkp_error_t result;

    ftn_binkp_command_init(&cmd_frame);
    result = ftn_binkp_command_create(&cmd_frame, BINKP_M_ERR, error_msg);
    if (result == BINKP_OK) {
        result = ftn_binkp_command_to_frame(&cmd_frame, frame);
    }

    ftn_binkp_command_free(&cmd_frame);
    return result;
}

ftn_binkp_error_t ftn_binkp_create_m_bsy(ftn_binkp_frame_t* frame, const char* reason) {
    ftn_binkp_command_frame_t cmd_frame;
    ftn_binkp_error_t result;

    ftn_binkp_command_init(&cmd_frame);
    result = ftn_binkp_command_create(&cmd_frame, BINKP_M_BSY, reason);
    if (result == BINKP_OK) {
        result = ftn_binkp_command_to_frame(&cmd_frame, frame);
    }

    ftn_binkp_command_free(&cmd_frame);
    return result;
}

ftn_binkp_error_t ftn_binkp_create_m_get(ftn_binkp_frame_t* frame, const char* filename, size_t offset) {
    char* escaped_filename;
    char args_buffer[512];
    ftn_binkp_command_frame_t cmd_frame;
    ftn_binkp_error_t result;

    if (!filename) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    result = ftn_binkp_escape_filename(filename, &escaped_filename);
    if (result != BINKP_OK) {
        return result;
    }

    snprintf(args_buffer, sizeof(args_buffer), "%s %lu", escaped_filename, (unsigned long)offset);

    ftn_binkp_command_init(&cmd_frame);
    result = ftn_binkp_command_create(&cmd_frame, BINKP_M_GET, args_buffer);
    if (result == BINKP_OK) {
        result = ftn_binkp_command_to_frame(&cmd_frame, frame);
    }

    ftn_binkp_command_free(&cmd_frame);
    free(escaped_filename);
    return result;
}

ftn_binkp_error_t ftn_binkp_create_m_skip(ftn_binkp_frame_t* frame, const char* filename, size_t offset) {
    char* escaped_filename;
    char args_buffer[512];
    ftn_binkp_command_frame_t cmd_frame;
    ftn_binkp_error_t result;

    if (!filename) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    result = ftn_binkp_escape_filename(filename, &escaped_filename);
    if (result != BINKP_OK) {
        return result;
    }

    snprintf(args_buffer, sizeof(args_buffer), "%s %lu", escaped_filename, (unsigned long)offset);

    ftn_binkp_command_init(&cmd_frame);
    result = ftn_binkp_command_create(&cmd_frame, BINKP_M_SKIP, args_buffer);
    if (result == BINKP_OK) {
        result = ftn_binkp_command_to_frame(&cmd_frame, frame);
    }

    ftn_binkp_command_free(&cmd_frame);
    free(escaped_filename);
    return result;
}

ftn_binkp_error_t ftn_binkp_parse_m_file(const ftn_binkp_command_frame_t* cmd_frame, ftn_binkp_file_info_t* file_info) {
    char* filename_start;
    char* space_pos;
    char* end_ptr;
    ftn_binkp_error_t result;

    if (!cmd_frame || !file_info || cmd_frame->cmd != BINKP_M_FILE || !cmd_frame->args) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    ftn_binkp_file_info_free(file_info);

    filename_start = cmd_frame->args;
    space_pos = strchr(filename_start, ' ');
    if (!space_pos) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* Extract and unescape filename */
    {
        char* temp_filename = malloc(space_pos - filename_start + 1);
        if (!temp_filename) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }
        strncpy(temp_filename, filename_start, space_pos - filename_start);
        temp_filename[space_pos - filename_start] = '\0';

        result = ftn_binkp_unescape_filename(temp_filename, &file_info->filename);
        free(temp_filename);
        if (result != BINKP_OK) {
            return result;
        }
    }

    /* Parse file size */
    file_info->file_size = strtoul(space_pos + 1, &end_ptr, 10);
    if (end_ptr == space_pos + 1 || *end_ptr != ' ') {
        ftn_binkp_file_info_free(file_info);
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* Parse timestamp */
    file_info->timestamp = strtoul(end_ptr + 1, &end_ptr, 10);
    if (*end_ptr != ' ' && *end_ptr != '\0') {
        ftn_binkp_file_info_free(file_info);
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* Parse offset (optional) */
    if (*end_ptr == ' ') {
        file_info->offset = strtoul(end_ptr + 1, NULL, 10);
    } else {
        file_info->offset = 0;
    }

    return BINKP_OK;
}

ftn_binkp_error_t ftn_binkp_parse_m_got(const ftn_binkp_command_frame_t* cmd_frame, char** filename, size_t* bytes_received) {
    char* space_pos;
    ftn_binkp_error_t result;

    if (!cmd_frame || !filename || !bytes_received || cmd_frame->cmd != BINKP_M_GOT || !cmd_frame->args) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    space_pos = strchr(cmd_frame->args, ' ');
    if (!space_pos) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* Extract and unescape filename */
    {
        char* temp_filename = malloc(space_pos - cmd_frame->args + 1);
        if (!temp_filename) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }
        strncpy(temp_filename, cmd_frame->args, space_pos - cmd_frame->args);
        temp_filename[space_pos - cmd_frame->args] = '\0';

        result = ftn_binkp_unescape_filename(temp_filename, filename);
        free(temp_filename);
        if (result != BINKP_OK) {
            return result;
        }
    }

    /* Parse bytes received */
    *bytes_received = strtoul(space_pos + 1, NULL, 10);

    return BINKP_OK;
}

ftn_binkp_error_t ftn_binkp_parse_m_get(const ftn_binkp_command_frame_t* cmd_frame, char** filename, size_t* offset) {
    char* space_pos;
    ftn_binkp_error_t result;

    if (!cmd_frame || !filename || !offset || cmd_frame->cmd != BINKP_M_GET || !cmd_frame->args) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    space_pos = strchr(cmd_frame->args, ' ');
    if (!space_pos) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* Extract and unescape filename */
    {
        char* temp_filename = malloc(space_pos - cmd_frame->args + 1);
        if (!temp_filename) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }
        strncpy(temp_filename, cmd_frame->args, space_pos - cmd_frame->args);
        temp_filename[space_pos - cmd_frame->args] = '\0';

        result = ftn_binkp_unescape_filename(temp_filename, filename);
        free(temp_filename);
        if (result != BINKP_OK) {
            return result;
        }
    }

    /* Parse offset */
    *offset = strtoul(space_pos + 1, NULL, 10);

    return BINKP_OK;
}

ftn_binkp_error_t ftn_binkp_parse_m_skip(const ftn_binkp_command_frame_t* cmd_frame, char** filename, size_t* offset) {
    char* space_pos;
    ftn_binkp_error_t result;

    if (!cmd_frame || !filename || !offset || cmd_frame->cmd != BINKP_M_SKIP || !cmd_frame->args) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    space_pos = strchr(cmd_frame->args, ' ');
    if (!space_pos) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    /* Extract and unescape filename */
    {
        char* temp_filename = malloc(space_pos - cmd_frame->args + 1);
        if (!temp_filename) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }
        strncpy(temp_filename, cmd_frame->args, space_pos - cmd_frame->args);
        temp_filename[space_pos - cmd_frame->args] = '\0';

        result = ftn_binkp_unescape_filename(temp_filename, filename);
        free(temp_filename);
        if (result != BINKP_OK) {
            return result;
        }
    }

    /* Parse offset */
    *offset = strtoul(space_pos + 1, NULL, 10);

    return BINKP_OK;
}

const char* ftn_binkp_command_name(ftn_binkp_command_t cmd) {
    switch (cmd) {
        case BINKP_M_NUL: return "M_NUL";
        case BINKP_M_ADR: return "M_ADR";
        case BINKP_M_PWD: return "M_PWD";
        case BINKP_M_FILE: return "M_FILE";
        case BINKP_M_OK: return "M_OK";
        case BINKP_M_EOB: return "M_EOB";
        case BINKP_M_GOT: return "M_GOT";
        case BINKP_M_ERR: return "M_ERR";
        case BINKP_M_BSY: return "M_BSY";
        case BINKP_M_GET: return "M_GET";
        case BINKP_M_SKIP: return "M_SKIP";
        default: return "UNKNOWN";
    }
}

ftn_binkp_error_t ftn_binkp_escape_filename(const char* filename, char** escaped) {
    size_t len;
    size_t i;
    size_t j;
    char* result;

    if (!filename || !escaped) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    len = strlen(filename);
    result = malloc(len * 4 + 1);
    if (!result) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    j = 0;
    for (i = 0; i < len; i++) {
        if (isprint((unsigned char)filename[i]) && filename[i] != ' ' && filename[i] != '\\') {
            result[j++] = filename[i];
        } else {
            sprintf(result + j, "\\x%02X", (unsigned char)filename[i]);
            j += 4;
        }
    }
    result[j] = '\0';

    *escaped = result;
    return BINKP_OK;
}

ftn_binkp_error_t ftn_binkp_unescape_filename(const char* escaped, char** filename) {
    size_t len;
    size_t i;
    size_t j;
    char* result;
    unsigned int hex_val;

    if (!escaped || !filename) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    len = strlen(escaped);
    result = malloc(len + 1);
    if (!result) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    j = 0;
    for (i = 0; i < len; i++) {
        if (escaped[i] == '\\' && i + 3 < len && escaped[i + 1] == 'x') {
            if (sscanf(escaped + i + 2, "%2X", &hex_val) == 1) {
                result[j++] = (char)hex_val;
                i += 3;
            } else {
                result[j++] = escaped[i];
            }
        } else {
            result[j++] = escaped[i];
        }
    }
    result[j] = '\0';

    *filename = result;
    return BINKP_OK;
}

ftn_binkp_error_t ftn_binkp_file_info_init(ftn_binkp_file_info_t* file_info) {
    if (!file_info) {
        return BINKP_ERROR_INVALID_COMMAND;
    }

    memset(file_info, 0, sizeof(ftn_binkp_file_info_t));
    return BINKP_OK;
}

void ftn_binkp_file_info_free(ftn_binkp_file_info_t* file_info) {
    if (file_info && file_info->filename) {
        free(file_info->filename);
        file_info->filename = NULL;
    }
}