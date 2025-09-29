/*
 * config.c - Configuration system implementation
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ftn.h"
#include "ftn/config.h"

/* String utility functions */
char* ftn_config_strdup(const char* str) {
    char* result;
    if (!str) return NULL;

    result = malloc(strlen(str) + 1);
    if (result) {
        strcpy(result, str);
    }
    return result;
}

void ftn_config_trim(char* str) {
    char *start, *end;

    if (!str) return;

    /* Trim leading whitespace */
    start = str;
    while (isspace((unsigned char)*start)) start++;

    /* Trim trailing whitespace */
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;

    /* Move string to beginning and null terminate */
    memmove(str, start, end - start + 1);
    str[end - start + 1] = '\0';
}

int ftn_config_strcasecmp(const char* s1, const char* s2) {
    if (!s1 || !s2) {
        return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    }

    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }

    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

char* ftn_config_parse_networks_list(const char* networks_str, size_t* count) {
    char* networks;
    char* network;
    char* saveptr;
    size_t network_count = 0;
    char* temp_str;

    if (!networks_str || !count) {
        if (count) *count = 0;
        return NULL;
    }

    temp_str = ftn_config_strdup(networks_str);
    if (!temp_str) {
        *count = 0;
        return NULL;
    }

    /* Count networks */
    network = strtok_r(temp_str, ",", &saveptr);
    while (network) {
        ftn_config_trim(network);
        if (strlen(network) > 0) {
            network_count++;
        }
        network = strtok_r(NULL, ",", &saveptr);
    }

    free(temp_str);

    if (network_count == 0) {
        *count = 0;
        return NULL;
    }

    /* Allocate network array */
    networks = malloc(network_count * sizeof(char*));
    if (!networks) {
        *count = 0;
        return NULL;
    }

    /* Parse networks again */
    temp_str = ftn_config_strdup(networks_str);
    if (!temp_str) {
        free(networks);
        *count = 0;
        return NULL;
    }

    network_count = 0;
    network = strtok_r(temp_str, ",", &saveptr);
    while (network) {
        ftn_config_trim(network);
        if (strlen(network) > 0) {
            ((char**)networks)[network_count] = ftn_config_strdup(network);
            if (!((char**)networks)[network_count]) {
                /* Cleanup on allocation failure */
                size_t i;
                for (i = 0; i < network_count; i++) {
                    free(((char**)networks)[i]);
                }
                free(networks);
                free(temp_str);
                *count = 0;
                return NULL;
            }
            network_count++;
        }
        network = strtok_r(NULL, ",", &saveptr);
    }

    free(temp_str);
    *count = network_count;
    return networks;
}

/* INI parsing functions */
ftn_config_ini_t* ftn_config_ini_new(void) {
    ftn_config_ini_t* ini = malloc(sizeof(ftn_config_ini_t));
    if (!ini) return NULL;

    ini->sections = NULL;
    ini->section_count = 0;
    ini->section_capacity = 0;

    return ini;
}

void ftn_config_ini_free(ftn_config_ini_t* ini) {
    size_t i, j;

    if (!ini) return;

    for (i = 0; i < ini->section_count; i++) {
        if (ini->sections[i].name) {
            free(ini->sections[i].name);
        }
        for (j = 0; j < ini->sections[i].pair_count; j++) {
            if (ini->sections[i].pairs[j].key) {
                free(ini->sections[i].pairs[j].key);
            }
            if (ini->sections[i].pairs[j].value) {
                free(ini->sections[i].pairs[j].value);
            }
        }
        if (ini->sections[i].pairs) {
            free(ini->sections[i].pairs);
        }
    }

    if (ini->sections) {
        free(ini->sections);
    }

    free(ini);
}

static ftn_error_t ftn_config_ini_add_section(ftn_config_ini_t* ini, const char* name) {
    ftn_config_section_t* new_sections;

    if (!ini || !name) return FTN_ERROR_INVALID_PARAMETER;

    /* Grow sections array if needed */
    if (ini->section_count >= ini->section_capacity) {
        size_t new_capacity = ini->section_capacity ? ini->section_capacity * 2 : 4;
        new_sections = realloc(ini->sections, new_capacity * sizeof(ftn_config_section_t));
        if (!new_sections) return FTN_ERROR_NOMEM;

        ini->sections = new_sections;
        ini->section_capacity = new_capacity;
    }

    /* Initialize new section */
    ini->sections[ini->section_count].name = ftn_config_strdup(name);
    if (!ini->sections[ini->section_count].name) return FTN_ERROR_NOMEM;

    ini->sections[ini->section_count].pairs = NULL;
    ini->sections[ini->section_count].pair_count = 0;
    ini->sections[ini->section_count].pair_capacity = 0;

    ini->section_count++;
    return FTN_OK;
}

static ftn_error_t ftn_config_ini_add_pair(ftn_config_ini_t* ini, const char* key, const char* value) {
    ftn_config_section_t* section;
    ftn_config_pair_t* new_pairs;

    if (!ini || !key || !value || ini->section_count == 0) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    section = &ini->sections[ini->section_count - 1];

    /* Grow pairs array if needed */
    if (section->pair_count >= section->pair_capacity) {
        size_t new_capacity = section->pair_capacity ? section->pair_capacity * 2 : 4;
        new_pairs = realloc(section->pairs, new_capacity * sizeof(ftn_config_pair_t));
        if (!new_pairs) return FTN_ERROR_NOMEM;

        section->pairs = new_pairs;
        section->pair_capacity = new_capacity;
    }

    /* Add new pair */
    section->pairs[section->pair_count].key = ftn_config_strdup(key);
    section->pairs[section->pair_count].value = ftn_config_strdup(value);

    if (!section->pairs[section->pair_count].key || !section->pairs[section->pair_count].value) {
        if (section->pairs[section->pair_count].key) {
            free(section->pairs[section->pair_count].key);
        }
        if (section->pairs[section->pair_count].value) {
            free(section->pairs[section->pair_count].value);
        }
        return FTN_ERROR_NOMEM;
    }

    section->pair_count++;
    return FTN_OK;
}

ftn_error_t ftn_config_ini_parse(ftn_config_ini_t* ini, const char* filename) {
    FILE* fp;
    char line[1024];
    char* trimmed_line;
    char* equals_pos;
    char key[256], value[768];
    int line_num = 0;

    if (!ini || !filename) return FTN_ERROR_INVALID_PARAMETER;

    fp = fopen(filename, "r");
    if (!fp) return FTN_ERROR_FILE_NOT_FOUND;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        trimmed_line = line;

        /* Trim the line */
        ftn_config_trim(trimmed_line);

        /* Skip empty lines and comments */
        if (strlen(trimmed_line) == 0 || trimmed_line[0] == '#' || trimmed_line[0] == ';') {
            continue;
        }

        /* Check for section header */
        if (trimmed_line[0] == '[') {
            char* close_bracket = strchr(trimmed_line, ']');
            if (close_bracket) {
                *close_bracket = '\0';
                if (ftn_config_ini_add_section(ini, trimmed_line + 1) != FTN_OK) {
                    fclose(fp);
                    return FTN_ERROR_NOMEM;
                }
            } else {
                fclose(fp);
                return FTN_ERROR_PARSE;
            }
            continue;
        }

        /* Check for key=value pair */
        equals_pos = strchr(trimmed_line, '=');
        if (equals_pos) {
            *equals_pos = '\0';
            strncpy(key, trimmed_line, sizeof(key) - 1);
            key[sizeof(key) - 1] = '\0';
            strncpy(value, equals_pos + 1, sizeof(value) - 1);
            value[sizeof(value) - 1] = '\0';

            ftn_config_trim(key);
            ftn_config_trim(value);

            if (ftn_config_ini_add_pair(ini, key, value) != FTN_OK) {
                fclose(fp);
                return FTN_ERROR_NOMEM;
            }
        } else {
            /* Invalid line format */
            fclose(fp);
            return FTN_ERROR_PARSE;
        }
    }

    fclose(fp);
    return FTN_OK;
}

const char* ftn_config_ini_get_value(const ftn_config_ini_t* ini, const char* section_name, const char* key_name) {
    size_t i, j;

    if (!ini || !section_name || !key_name) return NULL;

    /* Find section */
    for (i = 0; i < ini->section_count; i++) {
        if (ftn_config_strcasecmp(ini->sections[i].name, section_name) == 0) {
            /* Find key in section */
            for (j = 0; j < ini->sections[i].pair_count; j++) {
                if (ftn_config_strcasecmp(ini->sections[i].pairs[j].key, key_name) == 0) {
                    return ini->sections[i].pairs[j].value;
                }
            }
            break;
        }
    }

    return NULL;
}

int ftn_config_ini_has_section(const ftn_config_ini_t* ini, const char* section_name) {
    size_t i;

    if (!ini || !section_name) return 0;

    for (i = 0; i < ini->section_count; i++) {
        if (ftn_config_strcasecmp(ini->sections[i].name, section_name) == 0) {
            return 1;
        }
    }

    return 0;
}

/* Path templating functions */
char* ftn_config_expand_path(const char* template, const char* user, const char* network) {
    char* result;
    const char* src;
    char* dst;
    size_t result_len;
    size_t user_len = 0;
    size_t network_len = 0;

    if (!template) return NULL;

    if (user) user_len = strlen(user);
    if (network) network_len = strlen(network);

    /* Calculate maximum possible result length */
    result_len = strlen(template) + user_len + network_len + 1;
    result = malloc(result_len);
    if (!result) return NULL;

    src = template;
    dst = result;

    while (*src) {
        if (*src == '%') {
            if (strncmp(src, "%USER%", 6) == 0 && user) {
                strcpy(dst, user);
                dst += user_len;
                src += 6;
            } else if (strncmp(src, "%NETWORK%", 9) == 0 && network) {
                strcpy(dst, network);
                dst += network_len;
                src += 9;
            } else {
                *dst++ = *src++;
            }
        } else {
            *dst++ = *src++;
        }
    }

    *dst = '\0';
    return result;
}

/* Main configuration functions */
ftn_config_t* ftn_config_new(void) {
    ftn_config_t* config = malloc(sizeof(ftn_config_t));
    if (!config) return NULL;

    memset(config, 0, sizeof(ftn_config_t));
    return config;
}

void ftn_config_free(ftn_config_t* config) {
    size_t i;

    if (!config) return;

    /* Free node config */
    if (config->node) {
        if (config->node->name) free(config->node->name);
        if (config->node->sysop) free(config->node->sysop);
        if (config->node->sysop_name) free(config->node->sysop_name);
        if (config->node->email) free(config->node->email);
        if (config->node->www) free(config->node->www);
        if (config->node->telnet) free(config->node->telnet);
        if (config->node->networks) {
            for (i = 0; i < config->node->network_count; i++) {
                if (config->node->networks[i]) {
                    free(config->node->networks[i]);
                }
            }
            free(config->node->networks);
        }
        free(config->node);
    }

    /* Free news config */
    if (config->news) {
        if (config->news->path) free(config->news->path);
        free(config->news);
    }

    /* Free mail config */
    if (config->mail) {
        if (config->mail->inbox) free(config->mail->inbox);
        if (config->mail->outbox) free(config->mail->outbox);
        if (config->mail->sent) free(config->mail->sent);
        free(config->mail);
    }

    /* Free logging config */
    if (config->logging) {
        if (config->logging->level_str) free(config->logging->level_str);
        if (config->logging->log_file) free(config->logging->log_file);
        if (config->logging->ident) free(config->logging->ident);
        free(config->logging);
    }

    /* Free daemon config */
    if (config->daemon) {
        if (config->daemon->pid_file) free(config->daemon->pid_file);
        free(config->daemon);
    }

    /* Free network configs */
    if (config->networks) {
        for (i = 0; i < config->network_count; i++) {
            if (config->networks[i].section_name) free(config->networks[i].section_name);
            if (config->networks[i].name) free(config->networks[i].name);
            if (config->networks[i].domain) free(config->networks[i].domain);
            if (config->networks[i].address_str) free(config->networks[i].address_str);
            if (config->networks[i].hub_str) free(config->networks[i].hub_str);
            if (config->networks[i].inbox) free(config->networks[i].inbox);
            if (config->networks[i].outbox) free(config->networks[i].outbox);
            if (config->networks[i].processed) free(config->networks[i].processed);
            if (config->networks[i].bad) free(config->networks[i].bad);
            if (config->networks[i].duplicate_db) free(config->networks[i].duplicate_db);
            /* Free mailer-specific fields */
            if (config->networks[i].hub_hostname) free(config->networks[i].hub_hostname);
            if (config->networks[i].password) free(config->networks[i].password);
            if (config->networks[i].outbound_path) free(config->networks[i].outbound_path);
        }
        free(config->networks);
    }

    free(config);
}

static ftn_error_t ftn_config_load_node_section(ftn_config_t* config, const ftn_config_ini_t* ini) {
    const char* value;

    config->node = malloc(sizeof(ftn_node_config_t));
    if (!config->node) return FTN_ERROR_NOMEM;
    memset(config->node, 0, sizeof(ftn_node_config_t));

    /* Load node configuration */
    value = ftn_config_ini_get_value(ini, "node", "name");
    if (value) {
        config->node->name = ftn_config_strdup(value);
        if (!config->node->name) return FTN_ERROR_NOMEM;
    }

    value = ftn_config_ini_get_value(ini, "node", "sysop");
    if (value) {
        config->node->sysop = ftn_config_strdup(value);
        if (!config->node->sysop) return FTN_ERROR_NOMEM;
    }

    value = ftn_config_ini_get_value(ini, "node", "sysop_name");
    if (value) {
        config->node->sysop_name = ftn_config_strdup(value);
        if (!config->node->sysop_name) return FTN_ERROR_NOMEM;
    }

    value = ftn_config_ini_get_value(ini, "node", "email");
    if (value) {
        config->node->email = ftn_config_strdup(value);
        if (!config->node->email) return FTN_ERROR_NOMEM;
    }

    value = ftn_config_ini_get_value(ini, "node", "www");
    if (value) {
        config->node->www = ftn_config_strdup(value);
        if (!config->node->www) return FTN_ERROR_NOMEM;
    }

    value = ftn_config_ini_get_value(ini, "node", "telnet");
    if (value) {
        config->node->telnet = ftn_config_strdup(value);
        if (!config->node->telnet) return FTN_ERROR_NOMEM;
    }

    /* Parse networks list */
    value = ftn_config_ini_get_value(ini, "node", "networks");
    if (value) {
        config->node->networks = (char**)ftn_config_parse_networks_list(value, &config->node->network_count);
        if (!config->node->networks && config->node->network_count > 0) {
            return FTN_ERROR_NOMEM;
        }
    }

    return FTN_OK;
}

static ftn_error_t ftn_config_load_news_section(ftn_config_t* config, const ftn_config_ini_t* ini) {
    const char* value;

    if (!ftn_config_ini_has_section(ini, "news")) {
        return FTN_OK; /* Optional section */
    }

    config->news = malloc(sizeof(ftn_news_config_t));
    if (!config->news) return FTN_ERROR_NOMEM;
    memset(config->news, 0, sizeof(ftn_news_config_t));

    value = ftn_config_ini_get_value(ini, "news", "path");
    if (value) {
        config->news->path = ftn_config_strdup(value);
        if (!config->news->path) return FTN_ERROR_NOMEM;
    }

    return FTN_OK;
}

static ftn_error_t ftn_config_load_mail_section(ftn_config_t* config, const ftn_config_ini_t* ini) {
    const char* value;

    if (!ftn_config_ini_has_section(ini, "mail")) {
        return FTN_OK; /* Optional section */
    }

    config->mail = malloc(sizeof(ftn_mail_config_t));
    if (!config->mail) return FTN_ERROR_NOMEM;
    memset(config->mail, 0, sizeof(ftn_mail_config_t));

    value = ftn_config_ini_get_value(ini, "mail", "inbox");
    if (value) {
        config->mail->inbox = ftn_config_strdup(value);
        if (!config->mail->inbox) return FTN_ERROR_NOMEM;
    }

    value = ftn_config_ini_get_value(ini, "mail", "outbox");
    if (value) {
        config->mail->outbox = ftn_config_strdup(value);
        if (!config->mail->outbox) return FTN_ERROR_NOMEM;
    }

    value = ftn_config_ini_get_value(ini, "mail", "sent");
    if (value) {
        config->mail->sent = ftn_config_strdup(value);
        if (!config->mail->sent) return FTN_ERROR_NOMEM;
    }

    return FTN_OK;
}

static ftn_error_t ftn_config_load_logging_section(ftn_config_t* config, const ftn_config_ini_t* ini) {
    const char* value;

    if (!ftn_config_ini_has_section(ini, "logging")) {
        return FTN_OK; /* Optional section */
    }

    config->logging = malloc(sizeof(ftn_logging_config_t));
    if (!config->logging) return FTN_ERROR_NOMEM;
    memset(config->logging, 0, sizeof(ftn_logging_config_t));

    /* Set defaults */
    config->logging->level = FTN_LOG_INFO;

    value = ftn_config_ini_get_value(ini, "logging", "level");
    if (value) {
        config->logging->level_str = ftn_config_strdup(value);
        if (!config->logging->level_str) return FTN_ERROR_NOMEM;

        if (ftn_config_strcasecmp(value, "debug") == 0) {
            config->logging->level = FTN_LOG_DEBUG;
        } else if (ftn_config_strcasecmp(value, "info") == 0) {
            config->logging->level = FTN_LOG_INFO;
        } else if (ftn_config_strcasecmp(value, "warning") == 0) {
            config->logging->level = FTN_LOG_WARNING;
        } else if (ftn_config_strcasecmp(value, "error") == 0) {
            config->logging->level = FTN_LOG_ERROR;
        } else if (ftn_config_strcasecmp(value, "critical") == 0) {
            config->logging->level = FTN_LOG_CRITICAL;
        }
    }


    value = ftn_config_ini_get_value(ini, "logging", "log_file");
    if (value) {
        config->logging->log_file = ftn_config_strdup(value);
        if (!config->logging->log_file) return FTN_ERROR_NOMEM;
    }

    value = ftn_config_ini_get_value(ini, "logging", "ident");
    if (value) {
        config->logging->ident = ftn_config_strdup(value);
        if (!config->logging->ident) return FTN_ERROR_NOMEM;
    }

    return FTN_OK;
}

static ftn_error_t ftn_config_load_daemon_section(ftn_config_t* config, const ftn_config_ini_t* ini) {
    const char* value;

    if (!ftn_config_ini_has_section(ini, "daemon")) {
        return FTN_OK; /* Optional section */
    }

    config->daemon = malloc(sizeof(ftn_daemon_config_t));
    if (!config->daemon) return FTN_ERROR_NOMEM;
    memset(config->daemon, 0, sizeof(ftn_daemon_config_t));

    /* Set defaults */
    config->daemon->sleep_interval = 60;
    config->daemon->max_connections = 10;
    config->daemon->poll_interval = 300;

    value = ftn_config_ini_get_value(ini, "daemon", "pid_file");
    if (value) {
        config->daemon->pid_file = ftn_config_strdup(value);
        if (!config->daemon->pid_file) return FTN_ERROR_NOMEM;
    }

    value = ftn_config_ini_get_value(ini, "daemon", "sleep_interval");
    if (value) {
        config->daemon->sleep_interval = atoi(value);
        if (config->daemon->sleep_interval <= 0) {
            config->daemon->sleep_interval = 60;
        }
    }

    value = ftn_config_ini_get_value(ini, "daemon", "max_connections");
    if (value) {
        config->daemon->max_connections = atoi(value);
        if (config->daemon->max_connections <= 0) {
            config->daemon->max_connections = 10;
        }
    }

    value = ftn_config_ini_get_value(ini, "daemon", "poll_interval");
    if (value) {
        config->daemon->poll_interval = atoi(value);
        if (config->daemon->poll_interval <= 0) {
            config->daemon->poll_interval = 300;
        }
    }

    return FTN_OK;
}

static ftn_error_t ftn_config_load_network_sections(ftn_config_t* config, const ftn_config_ini_t* ini) {
    size_t i, network_count = 0;
    const char* value;

    /* Count network sections */
    for (i = 0; i < ini->section_count; i++) {
        if (ftn_config_strcasecmp(ini->sections[i].name, "node") != 0 &&
            ftn_config_strcasecmp(ini->sections[i].name, "news") != 0 &&
            ftn_config_strcasecmp(ini->sections[i].name, "mail") != 0 &&
            ftn_config_strcasecmp(ini->sections[i].name, "logging") != 0 &&
            ftn_config_strcasecmp(ini->sections[i].name, "daemon") != 0) {
            network_count++;
        }
    }

    if (network_count == 0) {
        return FTN_OK;
    }

    /* Allocate network array */
    config->networks = malloc(network_count * sizeof(ftn_network_config_t));
    if (!config->networks) return FTN_ERROR_NOMEM;
    memset(config->networks, 0, network_count * sizeof(ftn_network_config_t));

    /* Load network configurations */
    config->network_count = 0;
    for (i = 0; i < ini->section_count; i++) {
        if (ftn_config_strcasecmp(ini->sections[i].name, "node") != 0 &&
            ftn_config_strcasecmp(ini->sections[i].name, "news") != 0 &&
            ftn_config_strcasecmp(ini->sections[i].name, "mail") != 0 &&
            ftn_config_strcasecmp(ini->sections[i].name, "logging") != 0 &&
            ftn_config_strcasecmp(ini->sections[i].name, "daemon") != 0) {

            ftn_network_config_t* net = &config->networks[config->network_count];

            /* Store section name for lookup */
            net->section_name = ftn_config_strdup(ini->sections[i].name);
            if (!net->section_name) return FTN_ERROR_NOMEM;

            /* Load network settings */
            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "name");
            if (value) {
                net->name = ftn_config_strdup(value);
                if (!net->name) return FTN_ERROR_NOMEM;
            } else {
                /* Fallback to section name if no name field */
                net->name = ftn_config_strdup(ini->sections[i].name);
                if (!net->name) return FTN_ERROR_NOMEM;
            }

            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "domain");
            if (value) {
                net->domain = ftn_config_strdup(value);
                if (!net->domain) return FTN_ERROR_NOMEM;
            }

            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "address");
            if (value) {
                net->address_str = ftn_config_strdup(value);
                if (!net->address_str) return FTN_ERROR_NOMEM;
                ftn_address_parse(value, &net->address);
            }

            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "hub");
            if (value) {
                net->hub_str = ftn_config_strdup(value);
                if (!net->hub_str) return FTN_ERROR_NOMEM;
                ftn_address_parse(value, &net->hub);
            }

            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "inbox");
            if (value) {
                net->inbox = ftn_config_strdup(value);
                if (!net->inbox) return FTN_ERROR_NOMEM;
            }

            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "outbox");
            if (value) {
                net->outbox = ftn_config_strdup(value);
                if (!net->outbox) return FTN_ERROR_NOMEM;
            }

            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "processed");
            if (value) {
                net->processed = ftn_config_strdup(value);
                if (!net->processed) return FTN_ERROR_NOMEM;
            }

            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "bad");
            if (value) {
                net->bad = ftn_config_strdup(value);
                if (!net->bad) return FTN_ERROR_NOMEM;
            }

            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "duplicate_db");
            if (value) {
                net->duplicate_db = ftn_config_strdup(value);
                if (!net->duplicate_db) return FTN_ERROR_NOMEM;
            }

            /* Load mailer-specific settings */
            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "hub_hostname");
            if (value) {
                net->hub_hostname = ftn_config_strdup(value);
                if (!net->hub_hostname) return FTN_ERROR_NOMEM;
            }

            net->hub_port = 24554; /* Default binkp port */
            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "hub_port");
            if (value) {
                net->hub_port = atoi(value);
                if (net->hub_port <= 0 || net->hub_port > 65535) {
                    net->hub_port = 24554;
                }
            }

            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "password");
            if (value) {
                net->password = ftn_config_strdup(value);
                if (!net->password) return FTN_ERROR_NOMEM;
            }

            net->poll_frequency = 3600; /* Default 1 hour */
            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "poll_frequency");
            if (value) {
                net->poll_frequency = atoi(value);
                if (net->poll_frequency <= 0) {
                    net->poll_frequency = 3600;
                }
            }

            /* Boolean flags - default to 0 (false) */
            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "use_cram");
            net->use_cram = (value && (ftn_config_strcasecmp(value, "yes") == 0 ||
                           ftn_config_strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0)) ? 1 : 0;

            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "use_compression");
            net->use_compression = (value && (ftn_config_strcasecmp(value, "yes") == 0 ||
                                  ftn_config_strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0)) ? 1 : 0;

            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "use_crc");
            net->use_crc = (value && (ftn_config_strcasecmp(value, "yes") == 0 ||
                          ftn_config_strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0)) ? 1 : 0;

            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "use_nr_mode");
            net->use_nr_mode = (value && (ftn_config_strcasecmp(value, "yes") == 0 ||
                              ftn_config_strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0)) ? 1 : 0;

            value = ftn_config_ini_get_value(ini, ini->sections[i].name, "outbound_path");
            if (value) {
                net->outbound_path = ftn_config_strdup(value);
                if (!net->outbound_path) return FTN_ERROR_NOMEM;
            }

            config->network_count++;
        }
    }

    return FTN_OK;
}

ftn_error_t ftn_config_load(ftn_config_t* config, const char* filename) {
    ftn_config_ini_t* ini;
    ftn_error_t result;

    if (!config || !filename) return FTN_ERROR_INVALID_PARAMETER;

    ini = ftn_config_ini_new();
    if (!ini) return FTN_ERROR_NOMEM;

    result = ftn_config_ini_parse(ini, filename);
    if (result != FTN_OK) {
        ftn_config_ini_free(ini);
        return result;
    }

    /* Load each section */
    result = ftn_config_load_node_section(config, ini);
    if (result != FTN_OK) {
        ftn_config_ini_free(ini);
        return result;
    }

    result = ftn_config_load_news_section(config, ini);
    if (result != FTN_OK) {
        ftn_config_ini_free(ini);
        return result;
    }

    result = ftn_config_load_mail_section(config, ini);
    if (result != FTN_OK) {
        ftn_config_ini_free(ini);
        return result;
    }

    result = ftn_config_load_logging_section(config, ini);
    if (result != FTN_OK) {
        ftn_config_ini_free(ini);
        return result;
    }

    result = ftn_config_load_daemon_section(config, ini);
    if (result != FTN_OK) {
        ftn_config_ini_free(ini);
        return result;
    }

    result = ftn_config_load_network_sections(config, ini);
    if (result != FTN_OK) {
        ftn_config_ini_free(ini);
        return result;
    }

    ftn_config_ini_free(ini);
    return FTN_OK;
}

ftn_error_t ftn_config_validate(const ftn_config_t* config) {
    size_t i;

    if (!config) return FTN_ERROR_INVALID_PARAMETER;

    /* Node section is required */
    if (!config->node) return FTN_ERROR_INVALID;
    if (!config->node->name || strlen(config->node->name) == 0) return FTN_ERROR_INVALID;

    /* At least one network is required */
    if (config->network_count == 0 || !config->networks) return FTN_ERROR_INVALID;

    /* Validate each network */
    for (i = 0; i < config->network_count; i++) {
        const ftn_network_config_t* net = &config->networks[i];
        if (!net->section_name || strlen(net->section_name) == 0) return FTN_ERROR_INVALID;
        if (!net->address_str || strlen(net->address_str) == 0) return FTN_ERROR_INVALID;
        if (!net->inbox || strlen(net->inbox) == 0) return FTN_ERROR_INVALID;
        if (!net->outbox || strlen(net->outbox) == 0) return FTN_ERROR_INVALID;
    }

    return FTN_OK;
}

/* Accessor functions */
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

    if (!config || !name) return NULL;

    for (i = 0; i < config->network_count; i++) {
        if (config->networks[i].section_name && ftn_config_strcasecmp(config->networks[i].section_name, name) == 0) {
            return &config->networks[i];
        }
    }

    return NULL;
}

ftn_error_t ftn_config_reload(ftn_config_t* config, const char* filename) {
    ftn_config_t* new_config;
    ftn_error_t result;
    /* Save original data */
    ftn_node_config_t* old_node;
    ftn_news_config_t* old_news;
    ftn_mail_config_t* old_mail;
    ftn_logging_config_t* old_logging;
    ftn_daemon_config_t* old_daemon;
    ftn_network_config_t* old_networks;
    size_t old_network_count;

    if (!config || !filename) return FTN_ERROR_INVALID_PARAMETER;

    new_config = ftn_config_new();
    if (!new_config) return FTN_ERROR_NOMEM;

    result = ftn_config_load(new_config, filename);
    if (result != FTN_OK) {
        ftn_config_free(new_config);
        return result;
    }

    result = ftn_config_validate(new_config);
    if (result != FTN_OK) {
        ftn_config_free(new_config);
        return result;
    }

    old_node = config->node;
    old_news = config->news;
    old_mail = config->mail;
    old_logging = config->logging;
    old_daemon = config->daemon;
    old_networks = config->networks;
    old_network_count = config->network_count;

    /* Copy new data */
    config->node = new_config->node;
    config->news = new_config->news;
    config->mail = new_config->mail;
    config->logging = new_config->logging;
    config->daemon = new_config->daemon;
    config->networks = new_config->networks;
    config->network_count = new_config->network_count;

    /* Clean up new config structure without freeing the data */
    new_config->node = NULL;
    new_config->news = NULL;
    new_config->mail = NULL;
    new_config->logging = NULL;
    new_config->daemon = NULL;
    new_config->networks = NULL;
    new_config->network_count = 0;
    ftn_config_free(new_config);

    /* Free old data */
    if (old_node) {
        size_t i;
        if (old_node->name) free(old_node->name);
        if (old_node->sysop) free(old_node->sysop);
        if (old_node->sysop_name) free(old_node->sysop_name);
        if (old_node->email) free(old_node->email);
        if (old_node->www) free(old_node->www);
        if (old_node->telnet) free(old_node->telnet);
        if (old_node->networks) {
            for (i = 0; i < old_node->network_count; i++) {
                if (old_node->networks[i]) {
                    free(old_node->networks[i]);
                }
            }
            free(old_node->networks);
        }
        free(old_node);
    }

    if (old_news) {
        if (old_news->path) free(old_news->path);
        free(old_news);
    }

    if (old_mail) {
        if (old_mail->inbox) free(old_mail->inbox);
        if (old_mail->outbox) free(old_mail->outbox);
        if (old_mail->sent) free(old_mail->sent);
        free(old_mail);
    }

    if (old_logging) {
        if (old_logging->level_str) free(old_logging->level_str);
        if (old_logging->log_file) free(old_logging->log_file);
        if (old_logging->ident) free(old_logging->ident);
        free(old_logging);
    }

    if (old_daemon) {
        if (old_daemon->pid_file) free(old_daemon->pid_file);
        free(old_daemon);
    }

    if (old_networks) {
        size_t i;
        for (i = 0; i < old_network_count; i++) {
            if (old_networks[i].section_name) free(old_networks[i].section_name);
            if (old_networks[i].name) free(old_networks[i].name);
            if (old_networks[i].domain) free(old_networks[i].domain);
            if (old_networks[i].address_str) free(old_networks[i].address_str);
            if (old_networks[i].hub_str) free(old_networks[i].hub_str);
            if (old_networks[i].inbox) free(old_networks[i].inbox);
            if (old_networks[i].outbox) free(old_networks[i].outbox);
            if (old_networks[i].processed) free(old_networks[i].processed);
            if (old_networks[i].bad) free(old_networks[i].bad);
            if (old_networks[i].duplicate_db) free(old_networks[i].duplicate_db);
            if (old_networks[i].hub_hostname) free(old_networks[i].hub_hostname);
            if (old_networks[i].password) free(old_networks[i].password);
            if (old_networks[i].outbound_path) free(old_networks[i].outbound_path);
        }
        free(old_networks);
    }

    return FTN_OK;
}

ftn_error_t ftn_config_validate_mailer(const ftn_config_t* config) {
    size_t i;
    ftn_error_t result;

    if (!config) return FTN_ERROR_INVALID_PARAMETER;

    /* Validate basic configuration first */
    result = ftn_config_validate(config);
    if (result != FTN_OK) return result;

    /* Daemon section is required for mailer */
    if (!config->daemon) return FTN_ERROR_INVALID;

    /* Validate each network for mailer requirements */
    for (i = 0; i < config->network_count; i++) {
        const ftn_network_config_t* net = &config->networks[i];

        /* Hub hostname is required for mailer operation */
        if (!net->hub_hostname || strlen(net->hub_hostname) == 0) {
            return FTN_ERROR_INVALID;
        }

        /* Outbound path is required */
        if (!net->outbound_path || strlen(net->outbound_path) == 0) {
            return FTN_ERROR_INVALID;
        }
    }

    return FTN_OK;
}