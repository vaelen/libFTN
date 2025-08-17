/*
 * libFTN - RFC822 Message Format Support
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

#ifndef RFC822_H
#define RFC822_H

#include <time.h>

/* RFC822 Header Structure */
typedef struct {
    char* name;     /* Header field name */
    char* value;    /* Header field value */
} rfc822_header_t;

/* RFC822 Message Structure */
typedef struct {
    rfc822_header_t** headers;    /* Array of header pointers */
    size_t header_count;          /* Number of headers */
    size_t header_capacity;       /* Capacity of headers array */
    char* body;                   /* Message body */
} rfc822_message_t;

/* Message conversion functions */

/* Create and destroy RFC822 messages */
rfc822_message_t* rfc822_message_new(void);
void rfc822_message_free(rfc822_message_t* message);

/* Parse RFC822 message from text */
ftn_error_t rfc822_message_parse(const char* text, rfc822_message_t** message);

/* Generate RFC822 text from message */
char* rfc822_message_to_text(const rfc822_message_t* message);

/* Header manipulation functions */
ftn_error_t rfc822_message_add_header(rfc822_message_t* message, const char* name, const char* value);
const char* rfc822_message_get_header(const rfc822_message_t* message, const char* name);
ftn_error_t rfc822_message_set_header(rfc822_message_t* message, const char* name, const char* value);
ftn_error_t rfc822_message_remove_header(rfc822_message_t* message, const char* name);

/* Set message body */
ftn_error_t rfc822_message_set_body(rfc822_message_t* message, const char* body);

/* Conversion functions between FTN and RFC822 */

/* Convert FTN message to RFC822 */
ftn_error_t ftn_to_rfc822(const ftn_message_t* ftn_msg, const char* domain, rfc822_message_t** rfc_msg);

/* Convert RFC822 message to FTN */
ftn_error_t rfc822_to_ftn(const rfc822_message_t* rfc_msg, const char* domain, ftn_message_t** ftn_msg);

/* Utility functions */

/* Format FTN address for RFC822 */
char* ftn_address_to_rfc822(const ftn_address_t* addr, const char* name, const char* domain);

/* Parse RFC822 address to extract FTN address */
ftn_error_t rfc822_address_to_ftn(const char* rfc_addr, const char* domain, ftn_address_t* addr, char** name);

/* Convert FTN timestamp to RFC822 date format */
char* ftn_timestamp_to_rfc822(time_t timestamp);

/* Parse RFC822 date to timestamp */
ftn_error_t rfc822_date_to_timestamp(const char* date_str, time_t* timestamp);

/* Encode/decode message text for safe transport */
char* rfc822_encode_text(const char* text);
char* rfc822_decode_text(const char* text);

/* RFC1036 USENET conversion functions */

/* Convert FTN Echomail message to RFC1036 USENET article */
ftn_error_t ftn_to_usenet(const ftn_message_t* ftn_msg, const char* network, rfc822_message_t** usenet_msg);

/* Convert RFC1036 USENET article to FTN Echomail message */
ftn_error_t usenet_to_ftn(const rfc822_message_t* usenet_msg, const char* network, ftn_message_t** ftn_msg);

/* Generate newsgroup name from network and area */
char* ftn_area_to_newsgroup(const char* network, const char* area);

/* Extract area name from newsgroup */
char* newsgroup_to_ftn_area(const char* newsgroup, const char* network);

#endif /* RFC822_H */