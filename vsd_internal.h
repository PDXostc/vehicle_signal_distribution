// Copyright (C) 2018, Jaguar Land Rover
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (mfeuer1@jaguarlandrover.com)
//

#include <stdint.h>
#include <rmc_list.h>
#include "uthash.h"
#include "dstc.h"

#define vsd_data_u_nil ({ vsd_data_u res = {0}; res; })

struct vsd_signal;

RMC_LIST(vsd_subscriber_list, vsd_subscriber_node, vsd_subscriber_cb_t)
typedef vsd_subscriber_list vsd_subscriber_list_t;
typedef vsd_subscriber_node vsd_subscriber_node_t;

typedef struct vsd_signal {
    vsd_elem_type_e elem_type;
    vsd_data_type_e data_type;
    vsd_id_t id;
    char* name;
    char* description;
    struct vsd_signal_branch* parent;
    UT_hash_handle hh; // Make this struct hashable. https://troydhanson.github.io/uthash/
    // Subscribers can subscribe to any part of the tree, getting
    // a callback whenever one or more nodes in the tree have been updated.
    vsd_subscriber_list subscribers;
} vsd_signal_t;


typedef struct vsd_signal_branch {
    vsd_signal_t base;
    vsd_signal_list_t children;
} vsd_signal_branch_t;


typedef struct vsd_signal_leaf {
    vsd_signal_t base;
    vsd_data_u min;
    vsd_data_u max;
    vsd_data_u val;
} vsd_signal_leaf_t;


RMC_LIST(vsd_enum_list, vsd_enum_node, vsd_data_u)
typedef vsd_enum_list vsd_enum_list_t;
typedef vsd_enum_node vsd_enum_node_t;

typedef struct vsd_signal_enum {
    vsd_signal_leaf_t leaf;
    vsd_enum_list_t enums;
} vsd_signal_enum_t;

typedef struct vsd_context {
    vsd_signal_branch_t* root;
    vsd_signal_t* hash_table; // Hashed on id
    void* user_data;
} vsd_context_t;



extern int vsd_signal_init(vsd_context_t* ctx,
                         vsd_signal_t* sig,
                         vsd_elem_type_e elem_type,
                         vsd_data_type_e data_type,
                         vsd_signal_branch_t* parent,
                         vsd_id_t id,
                         char* name,
                         char* description);

extern int vsd_signal_create_branch(vsd_context_t* ctx,
                                  vsd_signal_branch_t** res,
                                  char* name,
                                  vsd_id_t id,
                                  char* description,
                                  vsd_signal_branch_t* parent);



extern int vsd_signal_create_leaf(vsd_context_t* ctx,
                                vsd_signal_leaf_t** res,
                                vsd_elem_type_e elem_type,
                                vsd_data_type_e data_type,
                                vsd_id_t id,
                                char* name,
                                char* description,
                                vsd_signal_branch_t* parent,
                                vsd_data_u min,
                                vsd_data_u max,
                                vsd_data_u val);


extern int vsd_signal_create_enum(vsd_context_t* ctx,
                                vsd_signal_enum_t** res,
                                vsd_elem_type_e elem_type,
                                vsd_data_type_e data_type,
                                vsd_id_t id,
                                char* name,
                                char* description,
                                vsd_signal_branch_t* parent,
                                vsd_data_u* enums, // Array of allowed values.
                                int enum_count,
                                vsd_data_u val);



extern int vsd_signal_delete(vsd_context_t* context, vsd_signal_t* desc);

extern int vsd_context_init(vsd_context_t* ctx);


extern int vsd_string_to_data(vsd_data_type_e type,
                              char* str,
                              vsd_data_u* res);
extern int vsd_publish(vsd_signal_t* sig);

extern int vsd_subscribe(vsd_context_t* ctx,
                         vsd_signal_t* sig,
                         vsd_subscriber_cb_t callback);

extern int vsd_unsubscribe(vsd_context_t* ctx,
                           vsd_signal_t* sig,
                           vsd_subscriber_cb_t callback);
extern void signal_transmit(vsd_id_t id, dstc_dynamic_data_t dynarg);
extern int vsd_transmit(vsd_id_t id, dstc_dynamic_data_t dynarg);
