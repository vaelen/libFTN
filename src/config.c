/*
 * config.c - Configuration system implementation (placeholder)
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ftn.h"
#include "ftn/config.h"

ftn_config_t* ftn_config_new(void) {
    ftn_config_t* config = malloc(sizeof(ftn_config_t));
    if (!config) {
        return NULL;
    }

    memset(config, 0, sizeof(ftn_config_t));
    return config;
}

void ftn_config_free(ftn_config_t* config) {
    if (!config) {
        return;
    }

    if (config->node) {
        if (config->node->name) free(config->node->name);
        if (config->node->sysop) free(config->node->sysop);
        if (config->node->sysop_name) free(config->node->sysop_name);
        if (config->node->email) free(config->node->email);
        if (config->node->www) free(config->node->www);
        if (config->node->telnet) free(config->node->telnet);
        if (config->node->networks) {
            size_t i;
            for (i = 0; i < config->node->network_count; i++) {
                if (config->node->networks[i]) {
                    free(config->node->networks[i]);
                }
            }
            free(config->node->networks);
        }
        free(config->node);
    }

    if (config->news) {
        if (config->news->path) free(config->news->path);
        free(config->news);
    }

    if (config->mail) {
        if (config->mail->inbox) free(config->mail->inbox);
        if (config->mail->outbox) free(config->mail->outbox);
        if (config->mail->sent) free(config->mail->sent);
        free(config->mail);
    }

    if (config->networks) {
        size_t i;
        for (i = 0; i < config->network_count; i++) {
            if (config->networks[i].name) free(config->networks[i].name);
            if (config->networks[i].domain) free(config->networks[i].domain);
            if (config->networks[i].address_str) free(config->networks[i].address_str);
            if (config->networks[i].hub_str) free(config->networks[i].hub_str);
            if (config->networks[i].inbox) free(config->networks[i].inbox);
            if (config->networks[i].outbox) free(config->networks[i].outbox);
            if (config->networks[i].processed) free(config->networks[i].processed);
            if (config->networks[i].bad) free(config->networks[i].bad);
            if (config->networks[i].duplicate_db) free(config->networks[i].duplicate_db);
        }
        free(config->networks);
    }

    free(config);
}

ftn_error_t ftn_config_load(ftn_config_t* config, const char* filename) {
    FILE* fp;

    if (!config || !filename) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    fp = fopen(filename, "r");
    if (!fp) {
        return FTN_ERROR_FILE_NOT_FOUND;
    }
    fclose(fp);

    config->node = malloc(sizeof(ftn_node_config_t));
    if (!config->node) {
        return FTN_ERROR_NOMEM;
    }
    memset(config->node, 0, sizeof(ftn_node_config_t));

    config->node->name = malloc(strlen("Test Node") + 1);
    if (config->node->name) {
        strcpy(config->node->name, "Test Node");
    }

    config->node->sysop = malloc(strlen("testuser") + 1);
    if (config->node->sysop) {
        strcpy(config->node->sysop, "testuser");
    }

    config->node->networks = malloc(sizeof(char*));
    if (config->node->networks) {
        config->node->networks[0] = malloc(strlen("testnet") + 1);
        if (config->node->networks[0]) {
            strcpy(config->node->networks[0], "testnet");
        }
        config->node->network_count = 1;
    }

    config->networks = malloc(sizeof(ftn_network_config_t));
    if (!config->networks) {
        return FTN_ERROR_NOMEM;
    }
    memset(config->networks, 0, sizeof(ftn_network_config_t));
    config->network_count = 1;

    config->networks[0].name = malloc(strlen("TestNet") + 1);
    if (config->networks[0].name) {
        strcpy(config->networks[0].name, "TestNet");
    }

    config->networks[0].domain = malloc(strlen("test.example.com") + 1);
    if (config->networks[0].domain) {
        strcpy(config->networks[0].domain, "test.example.com");
    }

    return FTN_OK;
}

ftn_error_t ftn_config_validate(const ftn_config_t* config) {
    if (!config) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    if (!config->node) {
        return FTN_ERROR_INVALID;
    }

    return FTN_OK;
}

char* ftn_config_expand_path(const char* template, const char* user, const char* network) {
    char* result;

    (void)user;    /* Suppress unused parameter warning */
    (void)network; /* Suppress unused parameter warning */

    if (!template) {
        return NULL;
    }

    result = malloc(strlen(template) + 1);
    if (result) {
        strcpy(result, template);
    }
    return result;
}

const ftn_node_config_t* ftn_config_get_node(const ftn_config_t* config) {
    return config ? config->node : NULL;
}

const ftn_mail_config_t* ftn_config_get_mail(const ftn_config_t* config) {
    return config ? config->mail : NULL;
}

const ftn_news_config_t* ftn_config_get_news(const ftn_config_t* config) {
    return config ? config->news : NULL;
}

const ftn_network_config_t* ftn_config_get_network(const ftn_config_t* config, const char* name) {
    size_t i;
    if (!config || !name) {
        return NULL;
    }

    for (i = 0; i < config->network_count; i++) {
        if (config->networks[i].name && strcmp(config->networks[i].name, name) == 0) {
            return &config->networks[i];
        }
    }

    return NULL;
}