/*
 * log.h - Logging system interface and function declarations
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

#ifndef FTN_LOG_H
#define FTN_LOG_H

#include <stdarg.h>
#include "log_levels.h"

/* Logging initialization and cleanup - using void* to avoid circular includes */
void ftn_log_init(const void* config);
void ftn_log_cleanup(void);

/* Logging level management */
ftn_log_level_t ftn_log_get_level(void);
void ftn_log_set_level(ftn_log_level_t level);

/* Logging functions */
void ftn_log(ftn_log_level_t level, const char* message);
void ftn_vlogf(ftn_log_level_t level, const char* format, va_list args);
void ftn_logf(ftn_log_level_t level, const char* format, ...);

/* Legacy macros for compatibility */
#define log_debug(message) ftn_log(FTN_LOG_DEBUG, message)
#define log_info(message) ftn_log(FTN_LOG_INFO, message)
#define log_warning(message) ftn_log(FTN_LOG_WARNING, message)
#define log_error(message) ftn_log(FTN_LOG_ERROR, message)
#define log_critical(message) ftn_log(FTN_LOG_CRITICAL, message)

void logf_debug(const char* format, ...);
void logf_info(const char* format, ...);
void logf_warning(const char* format, ...);
void logf_error(const char* format, ...);
void logf_critical(const char* format, ...);

#endif /* FTN_LOG_H */