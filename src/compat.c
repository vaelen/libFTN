/*
 * libFTN - Compatibility Layer Implementation
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include <ftn/compat.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
    strlcpy(buffer, temp_buffer, size);
    return result;
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

/* String comparison functions for non-POSIX environments */
#if !defined(_WIN32)

int strcasecmp(const char *s1, const char *s2) {
    if (!s1 || !s2) {
        return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    }
    
    while (*s1 && *s2) {
        char c1 = tolower((unsigned char)*s1);
        char c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    size_t i;
    
    if (!s1 || !s2) {
        return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    }
    
    for (i = 0; i < n && *s1 && *s2; i++, s1++, s2++) {
        char c1 = tolower((unsigned char)*s1);
        char c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
    }
    if (i == n) return 0;
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

#endif /* Non-Windows string comparison functions */

#endif /* POSIX compatibility functions */

/* String functions that aren't universally available */

/* BSD has strlcpy, but not all systems do */
#if !defined(__BSD__)

size_t strlcpy(char* dst, const char* src, size_t size) {
    size_t src_len;
    size_t copy_len;
    
    if (!src) {
        if (dst && size > 0) {
            *dst = '\0';
        }
        return 0;
    }
    
    src_len = strlen(src);
    
    if (!dst || size == 0) {
        return src_len;
    }
    
    /* Copy as much as will fit, leaving room for null terminator */
    copy_len = (src_len >= size) ? size - 1 : src_len;
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
    
    /* Return total length of source string */
    return src_len;
}

#endif

/* fnmatch implementation for non-POSIX systems */
#ifndef HAVE_FNMATCH_H

int fnmatch(const char *pattern, const char *string, int flags) {
    const char *p = pattern;
    const char *s = string;

    if (!pattern || !string) {
        return FNM_NOMATCH;
    }

    while (*p) {
        switch (*p) {
            case '*':
                /* Handle multiple asterisks */
                while (*p == '*') p++;

                /* If pattern ends with *, it matches */
                if (!*p) return 0;

                /* Try to match the rest of the pattern with each suffix of string */
                while (*s) {
                    if (fnmatch(p, s, flags) == 0) {
                        return 0;
                    }
                    s++;
                }
                return FNM_NOMATCH;

            case '?':
                /* ? matches any single character except '/' if FNM_PATHNAME is set */
                if (!*s) return FNM_NOMATCH;
                if ((flags & FNM_PATHNAME) && *s == '/') return FNM_NOMATCH;
                p++;
                s++;
                break;

            case '[':
                /* Character class - simplified implementation */
                p++; /* Skip '[' */
                {
                    int negate = 0;
                    int match = 0;

                    if (*p == '!' || *p == '^') {
                        negate = 1;
                        p++;
                    }

                    while (*p && *p != ']') {
                        if (*p == *s) {
                            match = 1;
                        }
                        p++;
                    }

                    if (*p == ']') p++; /* Skip ']' */

                    if ((!negate && !match) || (negate && match)) {
                        return FNM_NOMATCH;
                    }
                    s++;
                }
                break;

            case '\\':
                /* Escape character */
                if (!(flags & FNM_NOESCAPE)) {
                    p++;
                    if (!*p) return FNM_NOMATCH;
                }
                /* FALLTHROUGH */

            default:
                /* Regular character match */
                if ((flags & FNM_CASEFOLD)) {
                    if (tolower(*p) != tolower(*s)) {
                        return FNM_NOMATCH;
                    }
                } else {
                    if (*p != *s) {
                        return FNM_NOMATCH;
                    }
                }
                p++;
                s++;
                break;
        }
    }

    /* Pattern consumed, check if string is also consumed */
    return (*s == '\0') ? 0 : FNM_NOMATCH;
}

#endif /* HAVE_FNMATCH_H */

/* getopt implementation for non-POSIX systems */
#ifndef HAVE_GETOPT_H

char *optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = 0;

int getopt(int argc, char * const argv[], const char *optstring) {
    static int optpos = 1;
    const char *arg;
    const char *opt;

    /* Reset optarg */
    optarg = NULL;

    /* Check if we've processed all arguments */
    if (optind >= argc) {
        return -1;
    }

    arg = argv[optind];

    /* Check if this is an option */
    if (arg[0] != '-' || arg[1] == '\0') {
        return -1;
    }

    /* Handle "--" end of options marker */
    if (arg[1] == '-' && arg[2] == '\0') {
        optind++;
        return -1;
    }

    /* Get current option character */
    optopt = arg[optpos];

    /* Find option in optstring */
    opt = strchr(optstring, optopt);
    if (!opt) {
        if (opterr) {
            fprintf(stderr, "%s: illegal option -- %c\n", argv[0], optopt);
        }

        /* Move to next character or argument */
        if (arg[++optpos] == '\0') {
            optind++;
            optpos = 1;
        }
        return '?';
    }

    /* Check if option requires an argument */
    if (opt[1] == ':') {
        if (arg[optpos + 1] != '\0') {
            /* Argument attached to option */
            optarg = (char*)(arg + optpos + 1);
            optind++;
            optpos = 1;
        } else if (++optind < argc) {
            /* Argument in next argv element */
            optarg = argv[optind];
            optind++;
            optpos = 1;
        } else {
            /* Missing required argument */
            if (opterr) {
                fprintf(stderr, "%s: option requires an argument -- %c\n", argv[0], optopt);
            }
            optpos = 1;
            return (optstring[0] == ':') ? ':' : '?';
        }
    } else {
        /* No argument required */
        if (arg[++optpos] == '\0') {
            optind++;
            optpos = 1;
        }
    }

    return optopt;
}

int getopt_long(int argc, char * const argv[], const char *optstring,
                const struct option *longopts, int *longindex) {
    const char *arg;
    const struct option *opt;
    int i;

    /* Reset longindex if provided */
    if (longindex) {
        *longindex = -1;
    }

    /* Check if we've processed all arguments */
    if (optind >= argc) {
        return -1;
    }

    arg = argv[optind];

    /* Check if this is a long option */
    if (arg[0] == '-' && arg[1] == '-' && arg[2] != '\0') {
        const char *name = arg + 2;
        const char *equals = strchr(name, '=');
        size_t name_len = equals ? (size_t)(equals - name) : strlen(name);

        /* Find matching long option */
        for (i = 0; longopts[i].name; i++) {
            if (strncmp(longopts[i].name, name, name_len) == 0 &&
                strlen(longopts[i].name) == name_len) {

                opt = &longopts[i];
                if (longindex) {
                    *longindex = i;
                }

                /* Handle argument */
                if (opt->has_arg == required_argument) {
                    if (equals) {
                        optarg = (char*)(equals + 1);
                    } else if (++optind < argc) {
                        optarg = argv[optind];
                    } else {
                        if (opterr) {
                            fprintf(stderr, "%s: option '--%s' requires an argument\n",
                                    argv[0], opt->name);
                        }
                        optind++;
                        return '?';
                    }
                } else if (opt->has_arg == optional_argument) {
                    optarg = equals ? (char*)(equals + 1) : NULL;
                } else if (equals) {
                    if (opterr) {
                        fprintf(stderr, "%s: option '--%s' doesn't allow an argument\n",
                                argv[0], opt->name);
                    }
                    optind++;
                    return '?';
                }

                optind++;

                /* Set flag if provided */
                if (opt->flag) {
                    *opt->flag = opt->val;
                    return 0;
                } else {
                    return opt->val;
                }
            }
        }

        /* Long option not found */
        if (opterr) {
            fprintf(stderr, "%s: unrecognized option '--%.*s'\n",
                    argv[0], (int)name_len, name);
        }
        optind++;
        return '?';
    }

    /* Not a long option, fall back to regular getopt */
    return getopt(argc, argv, optstring);
}

#endif /* HAVE_GETOPT_H */
