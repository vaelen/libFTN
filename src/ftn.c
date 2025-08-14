/*
 * libFTN - FidoNet (FTN) Library Core Functions
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "ftn.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>


void ftn_trim(char* str) {
    char* start = str;
    char* end;
    
    if (!str || !*str) return;
    
    while (isspace(*start)) start++;
    
    if (*start == 0) {
        *str = 0;
        return;
    }
    
    end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) end--;
    
    end[1] = '\0';
    
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

int ftn_address_parse(const char* str, ftn_address_t* addr) {
    char* tmp;
    char* zone_str;
    char* net_str;
    char* node_str;
    char* point_str;
    
    if (!str || !addr) return 0;
    
    tmp = strdup(str);
    if (!tmp) return 0;
    
    addr->zone = 0;
    addr->net = 0;
    addr->node = 0;
    addr->point = 0;
    
    zone_str = strtok(tmp, ":");
    if (!zone_str) {
        free(tmp);
        return 0;
    }
    addr->zone = atoi(zone_str);
    
    net_str = strtok(NULL, "/");
    if (!net_str) {
        free(tmp);
        return 0;
    }
    addr->net = atoi(net_str);
    
    node_str = strtok(NULL, ".");
    if (!node_str) {
        free(tmp);
        return 0;
    }
    addr->node = atoi(node_str);
    
    point_str = strtok(NULL, "");
    if (point_str) {
        addr->point = atoi(point_str);
    }
    
    free(tmp);
    return 1;
}

int ftn_address_compare(const ftn_address_t* a, const ftn_address_t* b) {
    if (!a || !b) return -1;
    
    if (a->zone != b->zone) return (int)(a->zone - b->zone);
    if (a->net != b->net) return (int)(a->net - b->net);
    if (a->node != b->node) return (int)(a->node - b->node);
    if (a->point != b->point) return (int)(a->point - b->point);
    
    return 0;
}

void ftn_address_to_string(const ftn_address_t* addr, char* buffer, size_t size) {
    if (!addr || !buffer || size == 0) return;
    
    if (addr->point > 0) {
        snprintf(buffer, size, "%u:%u/%u.%u", 
                 addr->zone, addr->net, addr->node, addr->point);
    } else {
        snprintf(buffer, size, "%u:%u/%u", 
                 addr->zone, addr->net, addr->node);
    }
}
