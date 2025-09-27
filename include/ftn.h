/*
 * libFTN - FidoNet (FTN) Library
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

#ifndef FTN_H
#define FTN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ftn/compat.h"

#ifdef __STDC__
#define STDC89_COMPLIANT 1
#endif

/* FTN Error Codes */
typedef enum {
    FTN_OK = 0,
    FTN_ERROR_NOMEM,
    FTN_ERROR_FILE,
    FTN_ERROR_PARSE,
    FTN_ERROR_CRC,
    FTN_ERROR_INVALID,
    FTN_ERROR_NOTFOUND,
    FTN_ERROR_MEMORY,
    FTN_ERROR_INVALID_PARAMETER,
    FTN_ERROR_INVALID_FORMAT,
    FTN_ERROR_FILE_NOT_FOUND,
    FTN_ERROR_FILE_ACCESS
} ftn_error_t;

/* FTN Address Structure */
typedef struct {
    unsigned int zone;
    unsigned int net;
    unsigned int node;
    unsigned int point;
} ftn_address_t;

/* Include nodelist functionality */
#include "ftn/nodelist.h"
#include "ftn/packet.h"
#include "ftn/rfc822.h"
#include "ftn/version.h"
#include "ftn/config.h"
#include "ftn/router.h"
#include "ftn/storage.h"

/* Utility Functions */
void ftn_trim(char* str);
int ftn_address_parse(const char* str, ftn_address_t* addr);
int ftn_address_compare(const ftn_address_t* a, const ftn_address_t* b);
void ftn_address_to_string(const ftn_address_t* addr, char* buffer, size_t size);

#endif /* FTN_H */
