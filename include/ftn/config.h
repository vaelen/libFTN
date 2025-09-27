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
    char* name;
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
} ftn_network_config_t;

typedef struct {
    ftn_node_config_t* node;
    ftn_news_config_t* news;
    ftn_mail_config_t* mail;
    ftn_network_config_t* networks;
    size_t network_count;
} ftn_config_t;

ftn_config_t* ftn_config_new(void);
void ftn_config_free(ftn_config_t* config);
ftn_error_t ftn_config_load(ftn_config_t* config, const char* filename);
ftn_error_t ftn_config_validate(const ftn_config_t* config);

char* ftn_config_expand_path(const char* template, const char* user, const char* network);

const ftn_node_config_t* ftn_config_get_node(const ftn_config_t* config);
const ftn_mail_config_t* ftn_config_get_mail(const ftn_config_t* config);
const ftn_news_config_t* ftn_config_get_news(const ftn_config_t* config);
const ftn_network_config_t* ftn_config_get_network(const ftn_config_t* config, const char* name);

#endif /* FTN_CONFIG_H */