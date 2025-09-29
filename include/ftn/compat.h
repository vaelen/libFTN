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

/* Define BSD flag for platforms that have BSD-style functions */
#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__APPLE__)
#define __BSD__ 1
#endif

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
char* strtok_r(char* str, const char* delim, char** saveptr);

/* String comparison functions for non-POSIX environments */
#if defined(_WIN32)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
/* Provide our own implementations for platforms without these functions */
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
#endif

#endif /* POSIX compatibility */

/* String functions that aren't universally available */

/* BSD has strlcpy, but not all systems do */
#if !defined(__BSD__)
size_t strlcpy(char* dst, const char* src, size_t size);
#endif

/* fnmatch pattern matching */
#if defined(__has_include)
#  if __has_include(<fnmatch.h>)
#    define HAVE_FNMATCH_H
#  endif
#elif !defined(__STRICT_ANSI__) && !defined(_WIN32)
/* Assume POSIX systems have fnmatch.h unless strict ANSI mode */
#  define HAVE_FNMATCH_H
#endif

#ifdef HAVE_FNMATCH_H
#include <fnmatch.h>
#else
/* fnmatch flags */
#define FNM_NOMATCH 1
#define FNM_NOESCAPE 0x01
#define FNM_PATHNAME 0x02
#define FNM_PERIOD   0x04
#define FNM_CASEFOLD 0x08

int fnmatch(const char *pattern, const char *string, int flags);
#endif

/* getopt command line parsing */
#if defined(__has_include)
#  if __has_include(<getopt.h>)
#    define HAVE_GETOPT_H
#  endif
#elif !defined(__STRICT_ANSI__) && !defined(_WIN32)
/* Assume POSIX systems have getopt.h unless strict ANSI mode */
#  define HAVE_GETOPT_H
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
/* getopt variables */
extern char *optarg;
extern int optind, opterr, optopt;

/* getopt_long option structure */
struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

/* getopt_long argument values */
#define no_argument       0
#define required_argument 1
#define optional_argument 2

int getopt(int argc, char * const argv[], const char *optstring);
int getopt_long(int argc, char * const argv[], const char *optstring,
                const struct option *longopts, int *longindex);
#endif

#endif /* COMPAT_H */
