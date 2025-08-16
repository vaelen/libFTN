/*
 * libFTN Version Information Implementation
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "../include/version.h"

const char* ftn_get_version(void) {
    return FTN_VERSION_STRING;
}

const char* ftn_get_copyright(void) {
    return FTN_COPYRIGHT;
}

const char* ftn_get_license(void) {
    return FTN_LICENSE;
}