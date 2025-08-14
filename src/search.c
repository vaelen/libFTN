/*
 * libFTN - Nodelist Search Functions
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 */

#include "ftn.h"
#include <ctype.h>

static int strcasecmp_portable(const char* s1, const char* s2) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    int result;
    
    if (p1 == p2) return 0;
    
    while ((result = tolower(*p1) - tolower(*p2++)) == 0) {
        if (*p1++ == '\0') break;
    }
    
    return result;
}

ftn_nodelist_entry_t* ftn_nodelist_find_by_address(ftn_nodelist_t* nodelist, const ftn_address_t* address) {
    size_t i;
    
    if (!nodelist || !address) return NULL;
    
    for (i = 0; i < nodelist->count; i++) {
        if (ftn_address_compare(&nodelist->entries[i]->address, address) == 0) {
            return nodelist->entries[i];
        }
    }
    
    return NULL;
}

ftn_nodelist_entry_t* ftn_nodelist_find_by_name(ftn_nodelist_t* nodelist, const char* name) {
    size_t i;
    
    if (!nodelist || !name) return NULL;
    
    for (i = 0; i < nodelist->count; i++) {
        if (nodelist->entries[i]->name && 
            strcasecmp_portable(nodelist->entries[i]->name, name) == 0) {
            return nodelist->entries[i];
        }
    }
    
    return NULL;
}

ftn_nodelist_entry_t* ftn_nodelist_find_by_sysop(ftn_nodelist_t* nodelist, const char* sysop) {
    size_t i;
    
    if (!nodelist || !sysop) return NULL;
    
    for (i = 0; i < nodelist->count; i++) {
        if (nodelist->entries[i]->sysop && 
            strcasecmp_portable(nodelist->entries[i]->sysop, sysop) == 0) {
            return nodelist->entries[i];
        }
    }
    
    return NULL;
}

size_t ftn_nodelist_list_zones(ftn_nodelist_t* nodelist, unsigned int** zones) {
    size_t i, count = 0;
    unsigned int* zone_list = NULL;
    unsigned int* temp;
    int found;
    size_t j;
    
    if (!nodelist || !zones) return 0;
    
    *zones = NULL;
    
    for (i = 0; i < nodelist->count; i++) {
        if (nodelist->entries[i]->type != FTN_NODE_ZONE) continue;
        
        /* Check if zone already in list */
        found = 0;
        for (j = 0; j < count; j++) {
            if (zone_list[j] == nodelist->entries[i]->address.zone) {
                found = 1;
                break;
            }
        }
        
        if (!found) {
            temp = realloc(zone_list, (count + 1) * sizeof(unsigned int));
            if (!temp) {
                if (zone_list) free(zone_list);
                return 0;
            }
            zone_list = temp;
            zone_list[count] = nodelist->entries[i]->address.zone;
            count++;
        }
    }
    
    *zones = zone_list;
    return count;
}

size_t ftn_nodelist_list_nets(ftn_nodelist_t* nodelist, unsigned int zone, unsigned int** nets) {
    size_t i, count = 0;
    unsigned int* net_list = NULL;
    unsigned int* temp;
    int found;
    size_t j;
    
    if (!nodelist || !nets) return 0;
    
    *nets = NULL;
    
    for (i = 0; i < nodelist->count; i++) {
        if (nodelist->entries[i]->address.zone != zone) continue;
        if (nodelist->entries[i]->type != FTN_NODE_HOST && 
            nodelist->entries[i]->type != FTN_NODE_REGION) continue;
        
        /* Check if net already in list */
        found = 0;
        for (j = 0; j < count; j++) {
            if (net_list[j] == nodelist->entries[i]->address.net) {
                found = 1;
                break;
            }
        }
        
        if (!found) {
            temp = realloc(net_list, (count + 1) * sizeof(unsigned int));
            if (!temp) {
                if (net_list) free(net_list);
                return 0;
            }
            net_list = temp;
            net_list[count] = nodelist->entries[i]->address.net;
            count++;
        }
    }
    
    *nets = net_list;
    return count;
}

size_t ftn_nodelist_list_nodes(ftn_nodelist_t* nodelist, unsigned int zone, unsigned int net, ftn_nodelist_entry_t*** nodes) {
    size_t i, count = 0;
    ftn_nodelist_entry_t** node_list = NULL;
    ftn_nodelist_entry_t** temp;
    
    if (!nodelist || !nodes) return 0;
    
    *nodes = NULL;
    
    for (i = 0; i < nodelist->count; i++) {
        if (nodelist->entries[i]->address.zone != zone) continue;
        if (nodelist->entries[i]->address.net != net) continue;
        if (nodelist->entries[i]->type == FTN_NODE_ZONE ||
            nodelist->entries[i]->type == FTN_NODE_REGION ||
            nodelist->entries[i]->type == FTN_NODE_HOST) continue;
        
        temp = realloc(node_list, (count + 1) * sizeof(ftn_nodelist_entry_t*));
        if (!temp) {
            if (node_list) free(node_list);
            return 0;
        }
        node_list = temp;
        node_list[count] = nodelist->entries[i];
        count++;
    }
    
    *nodes = node_list;
    return count;
}
