/*
 * libFTN - Compatibility Layer
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

#ifndef COMPAT_H
#define COMPAT_H

#include <stdio.h>
#include <stdarg.h>

/* Compatibility layer for functions not available in ANSI C89 */

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L

/* C89 or earlier - provide our own implementations */
int snprintf(char* buffer, size_t size, const char* format, ...);
int vsnprintf(char* buffer, size_t size, const char* format, va_list args);

#endif /* C89 compatibility */

/* Compatibility layer for POSIX functions */

#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200809L

/* Non-POSIX environment - provide our own implementations */
char* strdup(const char* src);

#endif /* POSIX compatibility */

#endif /* COMPAT_H */
