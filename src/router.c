/*
 * router.c - Message routing engine implementation
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
#include "ftn/router.h"
#include "ftn/packet.h"
#include "ftn/config.h"
#include "ftn/dupecheck.h"

/* Default routing settings */
#define DEFAULT_RULE_CAPACITY 64

/* Utility functions */
char* ftn_router_strdup(const char* str) {
    char* result;
    if (!str) return NULL;

    result = malloc(strlen(str) + 1);
    if (result) {
        strcpy(result, str);
    }
    return result;
}

/* Router lifecycle */
ftn_router_t* ftn_router_new(const ftn_config_t* config, ftn_dupecheck_t* dupecheck) {
    ftn_router_t* router;

    if (!config) {
        return NULL;
    }

    router = malloc(sizeof(ftn_router_t));
    if (!router) {
        return NULL;
    }

    router->config = config;
    router->dupecheck = dupecheck;
    router->rule_count = 0;
    router->rule_capacity = DEFAULT_RULE_CAPACITY;

    router->rules = malloc(sizeof(ftn_routing_rule_t*) * router->rule_capacity);
    if (!router->rules) {
        free(router);
        return NULL;
    }

    return router;
}

void ftn_router_free(ftn_router_t* router) {
    size_t i;

    if (!router) return;

    if (router->rules) {
        for (i = 0; i < router->rule_count; i++) {
            ftn_routing_rule_free(router->rules[i]);
        }
        free(router->rules);
    }

    free(router);
}

/* Message analysis functions */
int ftn_router_is_netmail(const ftn_message_t* msg) {
    if (!msg) return 0;
    return (msg->type == FTN_MSG_NETMAIL);
}

int ftn_router_is_echomail(const ftn_message_t* msg) {
    if (!msg) return 0;
    return (msg->type == FTN_MSG_ECHOMAIL);
}

ftn_error_t ftn_router_get_message_area(const ftn_message_t* msg, char** area_name) {
    if (!msg || !area_name) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    *area_name = NULL;

    if (msg->area) {
        *area_name = ftn_router_strdup(msg->area);
        if (!*area_name) {
            return FTN_ERROR_NOMEM;
        }
    }

    return FTN_OK;
}

/* Address resolution functions */
ftn_error_t ftn_router_is_local_address(ftn_router_t* router, const ftn_address_t* addr, const char* network, int* is_local) {
    size_t i;
    const ftn_network_config_t* net_config;

    if (!router || !addr || !is_local) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    *is_local = 0;

    /* Check all configured networks */
    for (i = 0; i < router->config->network_count; i++) {
        net_config = &router->config->networks[i];

        /* If network is specified, check only that network */
        if (network && strcmp(net_config->section_name, network) != 0) {
            continue;
        }

        /* Check if address matches this network's address */
        if (addr->zone == net_config->address.zone &&
            addr->net == net_config->address.net &&
            addr->node == net_config->address.node &&
            addr->point == net_config->address.point) {
            *is_local = 1;
            return FTN_OK;
        }
    }

    return FTN_OK;
}

ftn_error_t ftn_router_find_network_for_address(ftn_router_t* router, const ftn_address_t* addr, char** network_name) {
    size_t i;
    const ftn_network_config_t* net_config;

    if (!router || !addr || !network_name) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    *network_name = NULL;

    /* Check all configured networks by zone */
    for (i = 0; i < router->config->network_count; i++) {
        net_config = &router->config->networks[i];

        /* Match by zone for now - could be enhanced with nodelist lookup */
        if (addr->zone == net_config->address.zone) {
            *network_name = ftn_router_strdup(net_config->section_name);
            if (!*network_name) {
                return FTN_ERROR_NOMEM;
            }
            return FTN_OK;
        }
    }

    return FTN_ERROR_NOTFOUND;
}

ftn_error_t ftn_router_analyze_message(ftn_router_t* router, const ftn_message_t* msg, ftn_destination_t* dest) {
    ftn_error_t result;
    int is_local;

    if (!router || !msg || !dest) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Initialize destination */
    memset(dest, 0, sizeof(ftn_destination_t));

    /* Copy destination address */
    dest->address = msg->dest_addr;

    /* Get area name for echomail */
    if (ftn_router_is_echomail(msg)) {
        result = ftn_router_get_message_area(msg, &dest->area_name);
        if (result != FTN_OK) {
            return result;
        }
    }

    /* Find network for address */
    result = ftn_router_find_network_for_address(router, &dest->address, &dest->network_name);
    if (result != FTN_OK && result != FTN_ERROR_NOTFOUND) {
        free(dest->area_name);
        return result;
    }

    /* Check if address is local */
    result = ftn_router_is_local_address(router, &dest->address, dest->network_name, &is_local);
    if (result != FTN_OK) {
        free(dest->area_name);
        free(dest->network_name);
        return result;
    }

    dest->is_local = is_local;
    return FTN_OK;
}

/* Pattern matching functions */
int ftn_router_pattern_match(const char* pattern, const char* text) {
    if (!pattern || !text) return 0;

    /* Use fnmatch for shell-style pattern matching */
    return (fnmatch(pattern, text, FNM_CASEFOLD) == 0);
}

int ftn_router_address_match(const char* pattern, const ftn_address_t* addr) {
    char addr_str[64];

    if (!pattern || !addr) return 0;

    /* Format address as string */
    snprintf(addr_str, sizeof(addr_str), "%d:%d/%d.%d",
             addr->zone, addr->net, addr->node, addr->point);

    return ftn_router_pattern_match(pattern, addr_str);
}

int ftn_router_area_match(const char* pattern, const char* area) {
    if (!pattern) return 0;
    if (!area) return (strcmp(pattern, "*") == 0);

    return ftn_router_pattern_match(pattern, area);
}

/* Routing decision utilities */
ftn_routing_decision_t* ftn_routing_decision_new(void) {
    ftn_routing_decision_t* decision;

    decision = malloc(sizeof(ftn_routing_decision_t));
    if (!decision) {
        return NULL;
    }

    memset(decision, 0, sizeof(ftn_routing_decision_t));
    return decision;
}

void ftn_routing_decision_free(ftn_routing_decision_t* decision) {
    if (!decision) return;

    free(decision->destination_path);
    free(decision->destination_user);
    free(decision->destination_area);
    free(decision->network_name);
    free(decision->reason);
    free(decision);
}

ftn_error_t ftn_routing_decision_set_local_mail(ftn_routing_decision_t* decision, const char* user, const char* path) {
    if (!decision) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    decision->action = FTN_ROUTE_LOCAL_MAIL;

    if (user) {
        decision->destination_user = ftn_router_strdup(user);
        if (!decision->destination_user) {
            return FTN_ERROR_NOMEM;
        }
    }

    if (path) {
        decision->destination_path = ftn_router_strdup(path);
        if (!decision->destination_path) {
            return FTN_ERROR_NOMEM;
        }
    }

    return FTN_OK;
}

ftn_error_t ftn_routing_decision_set_local_news(ftn_routing_decision_t* decision, const char* area, const char* path) {
    if (!decision) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    decision->action = FTN_ROUTE_LOCAL_NEWS;

    if (area) {
        decision->destination_area = ftn_router_strdup(area);
        if (!decision->destination_area) {
            return FTN_ERROR_NOMEM;
        }
    }

    if (path) {
        decision->destination_path = ftn_router_strdup(path);
        if (!decision->destination_path) {
            return FTN_ERROR_NOMEM;
        }
    }

    return FTN_OK;
}

ftn_error_t ftn_routing_decision_set_forward(ftn_routing_decision_t* decision, const ftn_address_t* addr, const char* network) {
    if (!decision || !addr) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    decision->action = FTN_ROUTE_FORWARD;
    decision->forward_to = *addr;

    if (network) {
        decision->network_name = ftn_router_strdup(network);
        if (!decision->network_name) {
            return FTN_ERROR_NOMEM;
        }
    }

    return FTN_OK;
}

ftn_error_t ftn_routing_decision_set_bounce(ftn_routing_decision_t* decision, const char* reason) {
    if (!decision) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    decision->action = FTN_ROUTE_BOUNCE;

    if (reason) {
        decision->reason = ftn_router_strdup(reason);
        if (!decision->reason) {
            return FTN_ERROR_NOMEM;
        }
    }

    return FTN_OK;
}

ftn_error_t ftn_routing_decision_set_drop(ftn_routing_decision_t* decision, const char* reason) {
    if (!decision) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    decision->action = FTN_ROUTE_DROP;

    if (reason) {
        decision->reason = ftn_router_strdup(reason);
        if (!decision->reason) {
            return FTN_ERROR_NOMEM;
        }
    }

    return FTN_OK;
}

/* Routing rule utilities */
ftn_routing_rule_t* ftn_routing_rule_new(void) {
    ftn_routing_rule_t* rule;

    rule = malloc(sizeof(ftn_routing_rule_t));
    if (!rule) {
        return NULL;
    }

    memset(rule, 0, sizeof(ftn_routing_rule_t));
    return rule;
}

void ftn_routing_rule_free(ftn_routing_rule_t* rule) {
    if (!rule) return;

    free(rule->name);
    free(rule->pattern);
    free(rule->parameter);
    free(rule);
}

ftn_error_t ftn_routing_rule_set(ftn_routing_rule_t* rule, const char* name, const char* pattern,
                                 ftn_routing_action_t action, const char* parameter, int priority) {
    if (!rule || !name || !pattern) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    rule->name = ftn_router_strdup(name);
    if (!rule->name) {
        return FTN_ERROR_NOMEM;
    }

    rule->pattern = ftn_router_strdup(pattern);
    if (!rule->pattern) {
        return FTN_ERROR_NOMEM;
    }

    rule->action = action;
    rule->priority = priority;

    if (parameter) {
        rule->parameter = ftn_router_strdup(parameter);
        if (!rule->parameter) {
            return FTN_ERROR_NOMEM;
        }
    }

    return FTN_OK;
}

/* Routing rules management */
ftn_error_t ftn_router_add_rule(ftn_router_t* router, const ftn_routing_rule_t* rule) {
    ftn_routing_rule_t** new_rules;
    ftn_routing_rule_t* new_rule;
    size_t i;

    if (!router || !rule) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Check if we need to expand the rules array */
    if (router->rule_count >= router->rule_capacity) {
        router->rule_capacity *= 2;
        new_rules = realloc(router->rules, sizeof(ftn_routing_rule_t*) * router->rule_capacity);
        if (!new_rules) {
            return FTN_ERROR_NOMEM;
        }
        router->rules = new_rules;
    }

    /* Create a copy of the rule */
    new_rule = ftn_routing_rule_new();
    if (!new_rule) {
        return FTN_ERROR_NOMEM;
    }

    if (ftn_routing_rule_set(new_rule, rule->name, rule->pattern, rule->action, rule->parameter, rule->priority) != FTN_OK) {
        ftn_routing_rule_free(new_rule);
        return FTN_ERROR_NOMEM;
    }

    /* Insert rule in priority order (lower priority number = higher priority) */
    for (i = router->rule_count; i > 0 && router->rules[i-1]->priority > new_rule->priority; i--) {
        router->rules[i] = router->rules[i-1];
    }
    router->rules[i] = new_rule;
    router->rule_count++;

    return FTN_OK;
}

ftn_error_t ftn_router_remove_rule(ftn_router_t* router, const char* rule_name) {
    size_t i, j;

    if (!router || !rule_name) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    for (i = 0; i < router->rule_count; i++) {
        if (strcmp(router->rules[i]->name, rule_name) == 0) {
            ftn_routing_rule_free(router->rules[i]);

            /* Shift remaining rules down */
            for (j = i; j < router->rule_count - 1; j++) {
                router->rules[j] = router->rules[j + 1];
            }
            router->rule_count--;
            return FTN_OK;
        }
    }

    return FTN_ERROR_NOTFOUND;
}

/* Utility functions */
char* ftn_router_format_address(const ftn_address_t* addr) {
    char* result;

    if (!addr) return NULL;

    result = malloc(64);
    if (!result) return NULL;

    snprintf(result, 64, "%d:%d/%d.%d", addr->zone, addr->net, addr->node, addr->point);
    return result;
}

const char* ftn_router_action_to_string(ftn_routing_action_t action) {
    switch (action) {
        case FTN_ROUTE_LOCAL_MAIL: return "LOCAL_MAIL";
        case FTN_ROUTE_LOCAL_NEWS: return "LOCAL_NEWS";
        case FTN_ROUTE_FORWARD:    return "FORWARD";
        case FTN_ROUTE_BOUNCE:     return "BOUNCE";
        case FTN_ROUTE_DROP:       return "DROP";
        default:                   return "UNKNOWN";
    }
}

ftn_routing_action_t ftn_router_string_to_action(const char* action_str) {
    if (!action_str) return FTN_ROUTE_DROP;

    if (strcmp(action_str, "LOCAL_MAIL") == 0) return FTN_ROUTE_LOCAL_MAIL;
    if (strcmp(action_str, "LOCAL_NEWS") == 0) return FTN_ROUTE_LOCAL_NEWS;
    if (strcmp(action_str, "FORWARD") == 0)    return FTN_ROUTE_FORWARD;
    if (strcmp(action_str, "BOUNCE") == 0)     return FTN_ROUTE_BOUNCE;
    if (strcmp(action_str, "DROP") == 0)       return FTN_ROUTE_DROP;

    return FTN_ROUTE_DROP; /* Default */
}

/* Main routing function */
ftn_error_t ftn_router_route_message(ftn_router_t* router, const ftn_message_t* msg, ftn_routing_decision_t* decision) {
    ftn_destination_t dest;
    ftn_error_t result;
    size_t i;
    int is_dupe = 0;
    char* addr_str = NULL;

    if (!router || !msg || !decision) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Check for duplicates first */
    if (router->dupecheck) {
        result = ftn_dupecheck_is_duplicate(router->dupecheck, msg, &is_dupe);
        if (result == FTN_OK && is_dupe) {
            return ftn_routing_decision_set_drop(decision, "Duplicate message");
        }
    }

    /* Analyze the message */
    result = ftn_router_analyze_message(router, msg, &dest);
    if (result != FTN_OK) {
        return result;
    }

    /* Apply routing rules in priority order */
    for (i = 0; i < router->rule_count; i++) {
        ftn_routing_rule_t* rule = router->rules[i];
        int match = 0;

        /* Check if rule pattern matches */
        if (strncmp(rule->pattern, "area:", 5) == 0) {
            /* Area pattern */
            if (dest.area_name) {
                match = ftn_router_area_match(rule->pattern + 5, dest.area_name);
            }
        } else if (strncmp(rule->pattern, "addr:", 5) == 0) {
            /* Address pattern */
            match = ftn_router_address_match(rule->pattern + 5, &dest.address);
        } else {
            /* Generic pattern - match against formatted address */
            addr_str = ftn_router_format_address(&dest.address);
            if (addr_str) {
                match = ftn_router_pattern_match(rule->pattern, addr_str);
                free(addr_str);
                addr_str = NULL;
            }
        }

        if (match) {
            /* Apply the matching rule */
            switch (rule->action) {
                case FTN_ROUTE_LOCAL_MAIL:
                    result = ftn_routing_decision_set_local_mail(decision, msg->to_user, rule->parameter);
                    break;
                case FTN_ROUTE_LOCAL_NEWS:
                    result = ftn_routing_decision_set_local_news(decision, dest.area_name, rule->parameter);
                    break;
                case FTN_ROUTE_FORWARD:
                    /* Parse forward address from parameter */
                    /* For now, just forward to original destination */
                    result = ftn_routing_decision_set_forward(decision, &dest.address, dest.network_name);
                    break;
                case FTN_ROUTE_BOUNCE:
                    result = ftn_routing_decision_set_bounce(decision, rule->parameter);
                    break;
                case FTN_ROUTE_DROP:
                    result = ftn_routing_decision_set_drop(decision, rule->parameter);
                    break;
            }

            /* Clean up and return */
            free(dest.area_name);
            free(dest.network_name);
            return result;
        }
    }

    /* No rules matched - apply default routing logic */
    if (ftn_router_is_netmail(msg)) {
        if (dest.is_local) {
            result = ftn_routing_decision_set_local_mail(decision, msg->to_user, NULL);
        } else {
            result = ftn_routing_decision_set_forward(decision, &dest.address, dest.network_name);
        }
    } else if (ftn_router_is_echomail(msg)) {
        if (dest.area_name) {
            result = ftn_routing_decision_set_local_news(decision, dest.area_name, NULL);
        } else {
            result = ftn_routing_decision_set_drop(decision, "No area specified");
        }
    } else {
        result = ftn_routing_decision_set_drop(decision, "Unknown message type");
    }

    /* Clean up */
    free(dest.area_name);
    free(dest.network_name);
    return result;
}

/* Address validation */
ftn_error_t ftn_router_validate_address(ftn_router_t* router, const ftn_address_t* addr, const char* network) {
    char* found_network = NULL;
    ftn_error_t result;

    if (!router || !addr) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    /* Basic address validation */
    if (addr->zone == 0 || addr->net == 0) {
        return FTN_ERROR_INVALID;
    }

    /* Check if address belongs to a known network */
    result = ftn_router_find_network_for_address(router, addr, &found_network);
    if (result == FTN_ERROR_NOTFOUND) {
        return FTN_ERROR_INVALID;
    } else if (result != FTN_OK) {
        return result;
    }

    /* If specific network was requested, verify it matches */
    if (network && found_network && strcmp(network, found_network) != 0) {
        free(found_network);
        return FTN_ERROR_INVALID;
    }

    free(found_network);
    return FTN_OK;
}