/*
 * config.h - Configuration system for libFTN
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

#ifndef FTN_CONFIG_H
#define FTN_CONFIG_H

#include "ftn.h"
#include "log_levels.h"

/* Configuration section structures */
typedef struct {
    char* name;
    char* sysop;
    char* sysop_name;
    char* email;
    char* www;
    char* telnet;
    char** networks;
    size_t network_count;
} ftn_node_config_t;

typedef struct {
    char* path;
} ftn_news_config_t;

typedef struct {
    char* inbox;
    char* outbox;
    char* sent;
} ftn_mail_config_t;



typedef struct {
    char* level_str;
    ftn_log_level_t level;
    char* log_file;
    char* ident;
} ftn_logging_config_t;

typedef struct {
    char* pid_file;
    int sleep_interval;
    int max_connections;        /* Maximum concurrent connections */
    int poll_interval;          /* Default polling interval in seconds */
} ftn_daemon_config_t;

typedef struct {
    char* section_name;         /* Section name for lookup */
    char* name;                 /* Display name from name field */
    char* domain;
    char* address_str;
    ftn_address_t address;
    char* hub_str;
    ftn_address_t hub;
    char* inbox;
    char* outbox;
    char* processed;
    char* bad;
    char* duplicate_db;
    /* Mailer-specific fields */
    char* hub_hostname;         /* TCP hostname for binkp connection */
    int hub_port;               /* TCP port (default 24554) */
    char* password;             /* Session password */
    int poll_frequency;         /* Poll interval in seconds */
    int use_cram;               /* Use CRAM authentication */
    int use_compression;        /* Enable compression */
    int use_crc;                /* Enable CRC verification */
    int use_nr_mode;            /* Enable Non-Reliable mode */
    char* outbound_path;        /* BSO outbound directory */
    /* PLZ compression settings */
    char* plz_mode_str;         /* PLZ mode string (none, supported, required) */
    int plz_mode;               /* PLZ mode as enum value */
    char* plz_level_str;        /* PLZ level string (fast, normal, best) */
    int plz_level;              /* PLZ level as enum value */
} ftn_network_config_t;

typedef struct {
    ftn_node_config_t* node;
    ftn_news_config_t* news;
    ftn_mail_config_t* mail;
    ftn_logging_config_t* logging;
    ftn_daemon_config_t* daemon;
    ftn_network_config_t* networks;
    size_t network_count;
} ftn_config_t;

/* INI parsing structures (internal) */
typedef struct {
    char* key;
    char* value;
} ftn_config_pair_t;

typedef struct {
    char* name;
    ftn_config_pair_t* pairs;
    size_t pair_count;
    size_t pair_capacity;
} ftn_config_section_t;

typedef struct {
    ftn_config_section_t* sections;
    size_t section_count;
    size_t section_capacity;
} ftn_config_ini_t;

/* Main configuration functions */
ftn_config_t* ftn_config_new(void);
void ftn_config_free(ftn_config_t* config);
ftn_error_t ftn_config_load(ftn_config_t* config, const char* filename);
ftn_error_t ftn_config_reload(ftn_config_t* config, const char* filename);
ftn_error_t ftn_config_validate(const ftn_config_t* config);
ftn_error_t ftn_config_validate_mailer(const ftn_config_t* config);

/* Path templating functions */
char* ftn_config_expand_path(const char* template, const char* user, const char* network);

/* Accessor functions for configuration sections */
const ftn_node_config_t* ftn_config_get_node(const ftn_config_t* config);
const ftn_mail_config_t* ftn_config_get_mail(const ftn_config_t* config);
const ftn_news_config_t* ftn_config_get_news(const ftn_config_t* config);
const ftn_network_config_t* ftn_config_get_network(const ftn_config_t* config, const char* name);

/* INI parsing functions (internal) */
ftn_config_ini_t* ftn_config_ini_new(void);
void ftn_config_ini_free(ftn_config_ini_t* ini);
ftn_error_t ftn_config_ini_parse(ftn_config_ini_t* ini, const char* filename);
const char* ftn_config_ini_get_value(const ftn_config_ini_t* ini, const char* section, const char* key);
int ftn_config_ini_has_section(const ftn_config_ini_t* ini, const char* section);

/* String utility functions */
char* ftn_config_strdup(const char* str);
void ftn_config_trim(char* str);
int ftn_config_strcasecmp(const char* s1, const char* s2);
char* ftn_config_parse_networks_list(const char* networks_str, size_t* count);

#endif /* FTN_CONFIG_H */
