/*
 * router.h - Message routing engine for libFTN
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

#ifndef FTN_ROUTER_H
#define FTN_ROUTER_H

#include "ftn.h"
#include "ftn/packet.h"
#include "ftn/config.h"
#include "ftn/dupecheck.h"

/* Routing action types */
typedef enum {
    FTN_ROUTE_LOCAL_MAIL,      /* Deliver to local user mailbox */
    FTN_ROUTE_LOCAL_NEWS,      /* Post to local newsgroup */
    FTN_ROUTE_FORWARD,         /* Forward to another node */
    FTN_ROUTE_BOUNCE,          /* Return to sender (undeliverable) */
    FTN_ROUTE_DROP             /* Discard message */
} ftn_routing_action_t;

/* Routing decision structure */
typedef struct {
    ftn_routing_action_t action;    /* What action to take */
    char* destination_path;         /* Path for local delivery */
    char* destination_user;         /* Username for mail delivery */
    char* destination_area;         /* Area name for news delivery */
    ftn_address_t forward_to;       /* Address for forwarding */
    char* network_name;             /* Network for routing */
    char* reason;                   /* Reason for routing decision */
} ftn_routing_decision_t;

/* Routing rule structure */
typedef struct {
    char* name;                     /* Rule name */
    char* pattern;                  /* Address/area pattern */
    ftn_routing_action_t action;    /* Action to take */
    char* parameter;                /* Action-specific parameter */
    int priority;                   /* Rule priority (lower = higher priority) */
} ftn_routing_rule_t;

/* Router structure */
typedef struct {
    const ftn_config_t* config;     /* Configuration system */
    ftn_dupecheck_t* dupecheck;     /* Duplicate detection */
    ftn_routing_rule_t** rules;     /* Array of routing rules */
    size_t rule_count;              /* Number of rules */
    size_t rule_capacity;           /* Capacity of rules array */
} ftn_router_t;

/* Destination information */
typedef struct {
    ftn_address_t address;          /* Destination address */
    char* area_name;                /* Echo area name (for echomail) */
    char* network_name;             /* Network name */
    int is_local;                   /* Whether destination is local */
} ftn_destination_t;

/* Router lifecycle */
ftn_router_t* ftn_router_new(const ftn_config_t* config, ftn_dupecheck_t* dupecheck);
void ftn_router_free(ftn_router_t* router);

/* Routing operations */
ftn_error_t ftn_router_route_message(ftn_router_t* router, const ftn_message_t* msg, ftn_routing_decision_t* decision);
ftn_error_t ftn_router_validate_address(ftn_router_t* router, const ftn_address_t* addr, const char* network);

/* Address resolution */
ftn_error_t ftn_router_resolve_destination(ftn_router_t* router, const ftn_message_t* msg, ftn_destination_t* dest);
ftn_error_t ftn_router_is_local_address(ftn_router_t* router, const ftn_address_t* addr, const char* network, int* is_local);

/* Routing rules management */
ftn_error_t ftn_router_add_rule(ftn_router_t* router, const ftn_routing_rule_t* rule);
ftn_error_t ftn_router_remove_rule(ftn_router_t* router, const char* rule_name);
ftn_error_t ftn_router_load_rules_from_config(ftn_router_t* router);

/* Message analysis */
ftn_error_t ftn_router_analyze_message(ftn_router_t* router, const ftn_message_t* msg, ftn_destination_t* dest);
ftn_error_t ftn_router_get_message_area(const ftn_message_t* msg, char** area_name);
int ftn_router_is_netmail(const ftn_message_t* msg);
int ftn_router_is_echomail(const ftn_message_t* msg);

/* Routing decision utilities */
ftn_routing_decision_t* ftn_routing_decision_new(void);
void ftn_routing_decision_free(ftn_routing_decision_t* decision);
ftn_error_t ftn_routing_decision_set_local_mail(ftn_routing_decision_t* decision, const char* user, const char* path);
ftn_error_t ftn_routing_decision_set_local_news(ftn_routing_decision_t* decision, const char* area, const char* path);
ftn_error_t ftn_routing_decision_set_forward(ftn_routing_decision_t* decision, const ftn_address_t* addr, const char* network);
ftn_error_t ftn_routing_decision_set_bounce(ftn_routing_decision_t* decision, const char* reason);
ftn_error_t ftn_routing_decision_set_drop(ftn_routing_decision_t* decision, const char* reason);

/* Routing rule utilities */
ftn_routing_rule_t* ftn_routing_rule_new(void);
void ftn_routing_rule_free(ftn_routing_rule_t* rule);
ftn_error_t ftn_routing_rule_set(ftn_routing_rule_t* rule, const char* name, const char* pattern,
                                 ftn_routing_action_t action, const char* parameter, int priority);

/* Pattern matching */
int ftn_router_pattern_match(const char* pattern, const char* text);
int ftn_router_address_match(const char* pattern, const ftn_address_t* addr);
int ftn_router_area_match(const char* pattern, const char* area);

/* Network utilities */
ftn_error_t ftn_router_find_network_for_address(ftn_router_t* router, const ftn_address_t* addr, char** network_name);
ftn_error_t ftn_router_get_local_networks(ftn_router_t* router, char*** networks, size_t* count);

/* Utility functions */
char* ftn_router_format_address(const ftn_address_t* addr);
ftn_error_t ftn_router_parse_rule_string(const char* rule_str, ftn_routing_rule_t* rule);
const char* ftn_router_action_to_string(ftn_routing_action_t action);
ftn_routing_action_t ftn_router_string_to_action(const char* action_str);

#endif /* FTN_ROUTER_H */