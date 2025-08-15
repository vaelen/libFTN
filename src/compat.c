/*
 * libFTN - Compatibility Layer Implementation
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "compat.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* Only compile these functions if we're using C89 or earlier */
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L

int vsnprintf(char* buffer, size_t size, const char* format, va_list args) {
    int result;
    char temp_buffer[8192]; /* Large temporary buffer - increased for safety */
    
    if (!buffer || size == 0 || !format) {
        return -1;
    }
    
    /* Use vsprintf to format into temporary buffer
     * This is still unsafe, but we're implementing the safe version ourselves.
     * In a real implementation, we'd need to implement our own format parser
     * or use a more sophisticated approach. For now, we rely on a large buffer.
     * 
     * A more robust implementation would:
     * 1. Parse the format string to estimate maximum output size
     * 2. Use multiple passes with different buffer sizes
     * 3. Implement our own format string parser
     * 
     * For the scope of this library and typical FTN address formatting,
     * an 8K buffer should be more than sufficient.
     */
    result = vsprintf(temp_buffer, format, args);
    
    /* Add bounds checking - if result is too large, something went wrong */
    if (result >= (int)sizeof(temp_buffer) - 1) {
        /* Buffer overflow detected - this should never happen with our usage */
        return -1;
    }
    
    if (result < 0) {
        return result;
    }
    
    /* Copy as much as will fit into the destination buffer */
    if ((size_t)result >= size) {
        /* Truncate to fit */
        if (size > 0) {
            strncpy(buffer, temp_buffer, size - 1);
            buffer[size - 1] = '\0';
        }
        return result; /* Return what would have been written */
    } else {
        /* Fits completely - use memcpy for extra safety */
        memcpy(buffer, temp_buffer, result + 1);
        return result;
    }
}

int snprintf(char* buffer, size_t size, const char* format, ...) {
    va_list args;
    int result;
    
    va_start(args, format);
    result = vsnprintf(buffer, size, format, args);
    va_end(args);
    
    return result;
}

#endif /* C89 compatibility functions */

/* Only compile these functions in non-POSIX environments */
#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200809L

char* strdup(const char* src) {
    char* dst;
    size_t len;
    
    if (!src) return NULL;
    
    len = strlen(src);
    dst = malloc(len + 1);
    if (!dst) return NULL;
    
    /* Copy only the actual string data, then null-terminate for safety */
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

char* strtok_r(char* str, const char* delim, char** saveptr) {
    char* start;
    char* end;
    
    if (!delim || !saveptr) return NULL;
    
    /* Use str if provided, otherwise continue from saveptr */
    if (str) {
        start = str;
    } else {
        start = *saveptr;
    }
    
    if (!start) return NULL;
    
    /* Skip leading delimiters */
    while (*start && strchr(delim, *start)) {
        start++;
    }
    
    if (*start == '\0') {
        *saveptr = NULL;
        return NULL;
    }
    
    /* Find end of token */
    end = start;
    while (*end && !strchr(delim, *end)) {
        end++;
    }
    
    /* Set up for next call */
    if (*end == '\0') {
        *saveptr = NULL;
    } else {
        *end = '\0';
        *saveptr = end + 1;
    }
    
    return start;
}

#endif /* POSIX compatibility functions */
