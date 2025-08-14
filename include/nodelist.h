/*
 * libFTN - FidoNet (FTN) Nodelist Support
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

#ifndef NODELIST_H
#define NODELIST_H

/* Forward declaration - we need the full definition from ftn.h */

/* Nodelist Entry Types (Keywords) */
typedef enum {
    FTN_NODE_NORMAL = 0,
    FTN_NODE_ZONE,
    FTN_NODE_REGION, 
    FTN_NODE_HOST,
    FTN_NODE_HUB,
    FTN_NODE_PVT,
    FTN_NODE_HOLD,
    FTN_NODE_DOWN
} ftn_node_type_t;

/* Comment Interest Flags */
typedef enum {
    FTN_COMMENT_NONE = 0,
    FTN_COMMENT_SYSOP = 1,
    FTN_COMMENT_USER = 2,
    FTN_COMMENT_FIDO = 4,
    FTN_COMMENT_ALL = 7,
    FTN_COMMENT_ERROR = 8
} ftn_comment_flags_t;

/* Nodelist Entry Structure */
typedef struct {
    ftn_node_type_t type;
    ftn_address_t address;
    char* name;
    char* location;
    char* sysop;
    char* phone;
    char* speed;
    char* flags;
} ftn_nodelist_entry_t;

/* Nodelist Structure */
typedef struct {
    char* title;
    unsigned int crc;
    ftn_nodelist_entry_t** entries;
    size_t count;
    size_t capacity;
} ftn_nodelist_t;

/* Nodelist Functions */

/* Load a nodelist from file */
ftn_error_t ftn_nodelist_load(const char* filename, ftn_nodelist_t** nodelist);

/* Free a nodelist */
void ftn_nodelist_free(ftn_nodelist_t* nodelist);

/* Search functions */
ftn_nodelist_entry_t* ftn_nodelist_find_by_address(ftn_nodelist_t* nodelist, const ftn_address_t* address);
ftn_nodelist_entry_t* ftn_nodelist_find_by_name(ftn_nodelist_t* nodelist, const char* name);
ftn_nodelist_entry_t* ftn_nodelist_find_by_sysop(ftn_nodelist_t* nodelist, const char* sysop);

/* Listing functions */
size_t ftn_nodelist_list_zones(ftn_nodelist_t* nodelist, unsigned int** zones);
size_t ftn_nodelist_list_nets(ftn_nodelist_t* nodelist, unsigned int zone, unsigned int** nets);
size_t ftn_nodelist_list_nodes(ftn_nodelist_t* nodelist, unsigned int zone, unsigned int net, ftn_nodelist_entry_t*** nodes);

/* Entry management */
ftn_nodelist_entry_t* ftn_nodelist_entry_new(void);
void ftn_nodelist_entry_free(ftn_nodelist_entry_t* entry);

/* Parsing functions */
ftn_error_t ftn_nodelist_parse_line(const char* line, ftn_nodelist_entry_t* entry);
ftn_error_t ftn_nodelist_parse_comment(const char* line, ftn_comment_flags_t* flags, char** text);

/* CRC calculation */
unsigned int ftn_crc16(const char* data, size_t length);
ftn_error_t ftn_nodelist_verify_crc(const char* filename, unsigned int expected_crc);

/* String conversion functions */
const char* ftn_node_type_to_string(ftn_node_type_t type);
ftn_node_type_t ftn_node_type_from_string(const char* str);

#endif /* NODELIST_H */