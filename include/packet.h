/*
 * libFTN - FidoNet (FTN) Packet and Message Support
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

#ifndef PACKET_H
#define PACKET_H

#include <time.h>

/* Message Types */
typedef enum {
    FTN_MSG_NETMAIL = 0,
    FTN_MSG_ECHOMAIL
} ftn_message_type_t;

/* Message Attributes (from FTS-0001) */
#define FTN_ATTR_PRIVATE        0x0001  /* Private */
#define FTN_ATTR_CRASH          0x0002  /* Crash */
#define FTN_ATTR_RECD           0x0004  /* Received */
#define FTN_ATTR_SENT           0x0008  /* Sent */
#define FTN_ATTR_FILEATTACH     0x0010  /* File Attached */
#define FTN_ATTR_INTRANSIT      0x0020  /* In Transit */
#define FTN_ATTR_ORPHAN         0x0040  /* Orphan */
#define FTN_ATTR_KILLSENT       0x0080  /* Kill Sent */
#define FTN_ATTR_LOCAL          0x0100  /* Local */
#define FTN_ATTR_HOLDFORPICKUP  0x0200  /* Hold For Pickup */
#define FTN_ATTR_UNUSED         0x0400  /* unused */
#define FTN_ATTR_FILEREQUEST    0x0800  /* File Request */
#define FTN_ATTR_RETRECREQ      0x1000  /* Return Receipt Request */
#define FTN_ATTR_ISRETRECEIPT   0x2000  /* Is Return Receipt */
#define FTN_ATTR_AUDITREQ       0x4000  /* Audit Request */
#define FTN_ATTR_FILEUPDREQ     0x8000  /* File Update Request */

/* Packet Header Structure (FTS-0001, 58 bytes) */
typedef struct {
    unsigned int orig_node;        /* Origin node */
    unsigned int dest_node;        /* Destination node */
    unsigned int year;             /* Year of packet creation */
    unsigned int month;            /* Month of packet creation (0-11) */
    unsigned int day;              /* Day of packet creation (1-31) */
    unsigned int hour;             /* Hour of packet creation (0-23) */
    unsigned int minute;           /* Minute of packet creation (0-59) */
    unsigned int second;           /* Second of packet creation (0-59) */
    unsigned int baud;             /* Max baud rate */
    unsigned int packet_type;      /* Packet type (should be 0x0002) */
    unsigned int orig_net;         /* Origin net */
    unsigned int dest_net;         /* Destination net */
    unsigned char prod_code;       /* Product code */
    unsigned char serial_no;       /* Serial number */
    char password[8];              /* Session password */
    unsigned int orig_zone;        /* Origin zone (optional) */
    unsigned int dest_zone;        /* Destination zone (optional) */
    char fill[20];                 /* Fill bytes */
} ftn_packet_header_t;

/* Packed Message Header Structure (FTS-0001) */
typedef struct {
    unsigned int message_type;     /* Message type (should be 0x0002) */
    unsigned int orig_node;        /* Origin node */
    unsigned int dest_node;        /* Destination node */
    unsigned int orig_net;         /* Origin net */
    unsigned int dest_net;         /* Destination net */
    unsigned int attributes;       /* Message attributes */
    unsigned int cost;             /* Message cost */
    char datetime[20];             /* Date/time string */
} ftn_packed_msg_header_t;

/* Message Structure */
typedef struct {
    ftn_message_type_t type;       /* Netmail or Echomail */
    ftn_address_t orig_addr;       /* Full origin address */
    ftn_address_t dest_addr;       /* Full destination address */
    unsigned int attributes;       /* Message attributes */
    unsigned int cost;             /* Message cost */
    time_t timestamp;              /* Message timestamp */
    char* to_user;                 /* To user name */
    char* from_user;               /* From user name */
    char* subject;                 /* Subject line */
    char* text;                    /* Message text */
    
    /* Echomail specific fields */
    char* area;                    /* Echo area name (AREA:) */
    char* origin;                  /* Origin line */
    char* tearline;                /* Tear line */
    char** seenby;                 /* SEEN-BY lines */
    size_t seenby_count;           /* Number of SEEN-BY lines */
    char** path;                   /* PATH lines */
    size_t path_count;             /* Number of PATH lines */
    
    /* Message ID and Reply (FTS-0009) */
    char* msgid;                   /* MSGID line */
    char* reply;                   /* REPLY line */
    
    /* Control Paragraphs (FTS-4000) */
    char** control_lines;          /* Generic control paragraphs */
    size_t control_count;          /* Number of control paragraphs */
    
    /* Addressing Control Paragraphs (FTS-4001) */
    unsigned int fmpt;             /* From point number (0 if none) */
    unsigned int topt;             /* To point number (0 if none) */
    char* intl;                    /* INTL addressing string */
    
    /* Time Zone Information (FTS-4008) */
    char* tzutc;                   /* TZUTC offset string */
    
    /* Netmail Tracking (FTS-4009) */
    char** via_lines;              /* Via tracking lines */
    size_t via_count;              /* Number of Via lines */
} ftn_message_t;

/* Packet Structure */
typedef struct {
    ftn_packet_header_t header;    /* Packet header */
    ftn_message_t** messages;      /* Array of message pointers */
    size_t message_count;          /* Number of messages */
    size_t message_capacity;       /* Capacity of messages array */
} ftn_packet_t;

/* Packet Functions */

/* Create and destroy packets */
ftn_packet_t* ftn_packet_new(void);
void ftn_packet_free(ftn_packet_t* packet);

/* Load and save packets */
ftn_error_t ftn_packet_load(const char* filename, ftn_packet_t** packet);
ftn_error_t ftn_packet_save(const char* filename, const ftn_packet_t* packet);

/* Add messages to packets */
ftn_error_t ftn_packet_add_message(ftn_packet_t* packet, ftn_message_t* message);

/* Message Functions */

/* Create and destroy messages */
ftn_message_t* ftn_message_new(ftn_message_type_t type);
void ftn_message_free(ftn_message_t* message);

/* Parse and create message text */
ftn_error_t ftn_message_parse_text(ftn_message_t* message, const char* text);
char* ftn_message_create_text(const ftn_message_t* message);

/* Echomail control line functions */
ftn_error_t ftn_message_add_seenby(ftn_message_t* message, const char* seenby);
ftn_error_t ftn_message_add_path(ftn_message_t* message, const char* path);
ftn_error_t ftn_message_set_msgid(ftn_message_t* message, const ftn_address_t* addr, const char* serial);
ftn_error_t ftn_message_set_reply(ftn_message_t* message, const char* reply_msgid);

/* Utility functions */
int ftn_message_has_attribute(const ftn_message_t* message, unsigned int attr);
void ftn_message_set_attribute(ftn_message_t* message, unsigned int attr);
void ftn_message_clear_attribute(ftn_message_t* message, unsigned int attr);

/* Date/time conversion functions */
ftn_error_t ftn_datetime_to_string(time_t timestamp, char* buffer, size_t size);
ftn_error_t ftn_datetime_from_string(const char* datetime_str, time_t* timestamp);

/* Control Paragraph Functions (FTS-4000) */
ftn_error_t ftn_message_add_control(ftn_message_t* message, const char* control_line);
const char* ftn_message_get_control(const ftn_message_t* message, const char* tag);

/* Addressing Control Paragraphs (FTS-4001) */
ftn_error_t ftn_message_set_fmpt(ftn_message_t* message, unsigned int point);
ftn_error_t ftn_message_set_topt(ftn_message_t* message, unsigned int point);
ftn_error_t ftn_message_set_intl(ftn_message_t* message, const ftn_address_t* dest, const ftn_address_t* orig);

/* Time Zone Information (FTS-4008) */
ftn_error_t ftn_message_set_tzutc(ftn_message_t* message, const char* offset);

/* Netmail Tracking (FTS-4009) */
ftn_error_t ftn_message_add_via(ftn_message_t* message, const ftn_address_t* addr, 
                                const char* timestamp, const char* program, const char* version);

#endif /* PACKET_H */
