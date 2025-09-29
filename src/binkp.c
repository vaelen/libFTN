#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "ftn/binkp.h"
#include "ftn/log.h"

ftn_binkp_error_t ftn_binkp_frame_init(ftn_binkp_frame_t* frame) {
    if (!frame) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    memset(frame, 0, sizeof(ftn_binkp_frame_t));
    return BINKP_OK;
}

void ftn_binkp_frame_free(ftn_binkp_frame_t* frame) {
    if (frame && frame->data) {
        free(frame->data);
        frame->data = NULL;
        frame->size = 0;
    }
}

ftn_binkp_error_t ftn_binkp_frame_parse(const uint8_t* buffer, size_t len, ftn_binkp_frame_t* frame) {
    uint16_t header_word;
    size_t frame_size;

    if (!buffer || !frame || len < BINKP_HEADER_SIZE) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    /* Parse header (network byte order) */
    header_word = (uint16_t)((buffer[0] << 8) | buffer[1]);
    frame->is_command = (header_word & BINKP_T_BIT) ? 1 : 0;
    frame_size = header_word & 0x7FFF;

    /* Validate frame size */
    if (frame_size > BINKP_MAX_FRAME_SIZE) {
        logf_error("Binkp frame size %zu exceeds maximum %d", frame_size, BINKP_MAX_FRAME_SIZE);
        return BINKP_ERROR_FRAME_TOO_LARGE;
    }

    /* Check if we have the complete frame */
    if (len < BINKP_HEADER_SIZE + frame_size) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    /* Store header */
    frame->header[0] = buffer[0];
    frame->header[1] = buffer[1];
    frame->size = frame_size;

    /* Allocate and copy data if present */
    if (frame_size > 0) {
        frame->data = malloc(frame_size);
        if (!frame->data) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(frame->data, buffer + BINKP_HEADER_SIZE, frame_size);
    } else {
        frame->data = NULL;
    }

    logf_debug("Parsed binkp frame: size=%zu, command=%d", frame_size, frame->is_command);
    return BINKP_OK;
}

ftn_binkp_error_t ftn_binkp_frame_create(ftn_binkp_frame_t* frame, int is_command, const uint8_t* data, size_t size) {
    uint16_t header_word;

    if (!frame) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    if (size > BINKP_MAX_FRAME_SIZE) {
        logf_error("Binkp frame size %zu exceeds maximum %d", size, BINKP_MAX_FRAME_SIZE);
        return BINKP_ERROR_FRAME_TOO_LARGE;
    }

    /* Free existing data */
    ftn_binkp_frame_free(frame);

    /* Create header (network byte order) */
    header_word = (uint16_t)size;
    if (is_command) {
        header_word |= BINKP_T_BIT;
    }

    frame->header[0] = (uint8_t)((header_word >> 8) & 0xFF);
    frame->header[1] = (uint8_t)(header_word & 0xFF);
    frame->is_command = is_command ? 1 : 0;
    frame->size = size;

    /* Copy data if present */
    if (size > 0 && data) {
        frame->data = malloc(size);
        if (!frame->data) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }
        memcpy(frame->data, data, size);
    } else {
        frame->data = NULL;
    }

    logf_debug("Created binkp frame: size=%zu, command=%d", size, is_command);
    return BINKP_OK;
}

ftn_binkp_error_t ftn_binkp_frame_serialize(const ftn_binkp_frame_t* frame, uint8_t* buffer, size_t buffer_size, size_t* bytes_written) {
    size_t total_size;

    if (!frame || !buffer || !bytes_written) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    total_size = BINKP_HEADER_SIZE + frame->size;

    if (buffer_size < total_size) {
        return BINKP_ERROR_BUFFER_TOO_SMALL;
    }

    /* Copy header */
    buffer[0] = frame->header[0];
    buffer[1] = frame->header[1];

    /* Copy data if present */
    if (frame->size > 0 && frame->data) {
        memcpy(buffer + BINKP_HEADER_SIZE, frame->data, frame->size);
    }

    *bytes_written = total_size;
    return BINKP_OK;
}

ftn_binkp_error_t ftn_binkp_frame_send(ftn_net_connection_t* conn, const ftn_binkp_frame_t* frame) {
    uint8_t buffer[BINKP_MAX_FRAME_SIZE + BINKP_HEADER_SIZE];
    size_t bytes_written;
    ftn_binkp_error_t result;
    ftn_error_t net_result;

    if (!conn || !frame) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    result = ftn_binkp_frame_serialize(frame, buffer, sizeof(buffer), &bytes_written);
    if (result != BINKP_OK) {
        return result;
    }

    net_result = ftn_net_send_all(conn, buffer, bytes_written);
    if (net_result != FTN_OK) {
        logf_error("Failed to send binkp frame: network error");
        return BINKP_ERROR_NETWORK;
    }

    logf_debug("Sent binkp frame: %zu bytes", bytes_written);
    return BINKP_OK;
}

ftn_binkp_error_t ftn_binkp_frame_receive(ftn_net_connection_t* conn, ftn_binkp_frame_t* frame, int timeout_ms) {
    uint8_t header[BINKP_HEADER_SIZE];
    uint16_t header_word;
    size_t frame_size;
    ftn_error_t net_result;

    if (!conn || !frame) {
        return BINKP_ERROR_INVALID_FRAME;
    }

    /* Set receive timeout */
    if (timeout_ms > 0) {
        ftn_net_set_timeout(conn, timeout_ms);
    }

    /* Receive header */
    net_result = ftn_net_recv_all(conn, header, BINKP_HEADER_SIZE);
    if (net_result != FTN_OK) {
        if (net_result == FTN_ERROR_TIMEOUT) {
            return BINKP_ERROR_TIMEOUT;
        }
        logf_error("Failed to receive binkp frame header: network error");
        return BINKP_ERROR_NETWORK;
    }

    /* Parse frame size */
    header_word = (uint16_t)((header[0] << 8) | header[1]);
    frame_size = header_word & 0x7FFF;

    if (frame_size > BINKP_MAX_FRAME_SIZE) {
        logf_error("Received binkp frame size %zu exceeds maximum %d", frame_size, BINKP_MAX_FRAME_SIZE);
        return BINKP_ERROR_FRAME_TOO_LARGE;
    }

    /* Initialize frame */
    ftn_binkp_frame_free(frame);
    frame->header[0] = header[0];
    frame->header[1] = header[1];
    frame->is_command = (header_word & BINKP_T_BIT) ? 1 : 0;
    frame->size = frame_size;

    /* Receive data if present */
    if (frame_size > 0) {
        frame->data = malloc(frame_size);
        if (!frame->data) {
            return BINKP_ERROR_BUFFER_TOO_SMALL;
        }

        net_result = ftn_net_recv_all(conn, frame->data, frame_size);
        if (net_result != FTN_OK) {
            free(frame->data);
            frame->data = NULL;
            if (net_result == FTN_ERROR_TIMEOUT) {
                return BINKP_ERROR_TIMEOUT;
            }
            logf_error("Failed to receive binkp frame data: network error");
            return BINKP_ERROR_NETWORK;
        }
    } else {
        frame->data = NULL;
    }

    logf_debug("Received binkp frame: size=%zu, command=%d", frame_size, frame->is_command);
    return BINKP_OK;
}

size_t ftn_binkp_frame_total_size(const ftn_binkp_frame_t* frame) {
    if (!frame) {
        return 0;
    }
    return BINKP_HEADER_SIZE + frame->size;
}

int ftn_binkp_frame_is_command(const ftn_binkp_frame_t* frame) {
    if (!frame) {
        return 0;
    }
    return frame->is_command;
}

const char* ftn_binkp_error_string(ftn_binkp_error_t error) {
    switch (error) {
        case BINKP_OK:
            return "Success";
        case BINKP_ERROR_INVALID_FRAME:
            return "Invalid frame";
        case BINKP_ERROR_FRAME_TOO_LARGE:
            return "Frame too large";
        case BINKP_ERROR_BUFFER_TOO_SMALL:
            return "Buffer too small";
        case BINKP_ERROR_INVALID_COMMAND:
            return "Invalid command";
        case BINKP_ERROR_NETWORK:
            return "Network error";
        case BINKP_ERROR_TIMEOUT:
            return "Timeout";
        case BINKP_ERROR_AUTH_FAILED:
            return "Authentication failed";
        case BINKP_ERROR_PROTOCOL_ERROR:
            return "Protocol error";
        default:
            return "Unknown error";
    }
}