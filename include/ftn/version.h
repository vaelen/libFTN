/*
 * libFTN Version Information
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#ifndef FTN_VERSION_H
#define FTN_VERSION_H

#define FTN_VERSION_MAJOR 1
#define FTN_VERSION_MINOR 0
#define FTN_VERSION_PATCH 0
#define FTN_VERSION_STRING "1.0.0"

#define FTN_COPYRIGHT "Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>"
#define FTN_LICENSE "MIT"

/* Get version information */
const char* ftn_get_version(void);
const char* ftn_get_copyright(void);
const char* ftn_get_license(void);

#endif /* FTN_VERSION_H */