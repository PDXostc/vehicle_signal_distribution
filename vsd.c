// Copyright (C) 2018, Jaguar Land Rover
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (mfeuer1@jaguarlandrover.com)
//
// Common signal functions
//
#include "vsd.h"
#include "vsd_internal.h"
#include <memory.h>
#include <string.h>
#include <errno.h>
#include "rmc_common.h"
#include "rmc_list_template.h"
#include "rmc_log.h"
#include "dstc.h"

DSTC_CLIENT(vsd_signal_transmit, uint32_t,, DECL_DYNAMIC_ARG )
DSTC_SERVER(vsd_signal_transmit, uint32_t,, DECL_DYNAMIC_ARG )

RMC_LIST_IMPL(vsd_desc_list, vsd_desc_node, vsd_desc_t*)
RMC_LIST_IMPL(vsd_enum_list, vsd_enum_node, vsd_data_u)
RMC_LIST_IMPL(vsd_subscriber_list, vsd_subscriber_node, vsd_subscriber_cb_t)

static vsd_context_t* _current_context = 0;

static char* _enum_data_type[] = {
    "int8",       // vsd_int8 = 0,
    "uint8",      // vsd_uint8 = 1,
    "int16",      // vsd_int16 = 2,
    "uint16",     // vsd_uint16 = 3,
    "int32",      // vsd_int32 = 4,
    "uint32",     // vsd_uint32 = 5,
    "double",     // vsd_double = 6,
    "float",      // vsd_float = 7,
    "boolean",    // vsd_boolean = 8,
    "string",     // vsd_string = 9,
    "stream",     // vsd_stream = 10,
    "unknown",     // vsd_stream = 11,
};

static char* _enum_elem_type[] = {
    "attribute",  // vsd_attribute = 0,
    "branch",     // vsd_branch = 1,
    "sensor",     // vsd_sensor = 2,
    "actuator",   // vsd_actuator = 3,
    "rbranch",    // vsd_rbranch = 4,
    "element",    // vsd_element = 5,
};

static int _data_type_size[] =
{
    sizeof(int8_t),
    sizeof(uint8_t),
    sizeof(int16_t),
    sizeof(uint16_t),
    sizeof(int32_t),
    sizeof(uint32_t),
    sizeof(double),
    sizeof(float),
    1,  // boolean
    -1, // string
    -1, // stream
    -1, // not applicable
};

static int vsd_data_copy(vsd_data_u* dst,
                         vsd_data_u* src,
                         vsd_data_type_e data_type)
{
    switch(data_type) {
    case vsd_string:
        if (dst->s.allocated < src->s.len) {
            // Free old memory, if set. free(3) takes null pointer.
            free(dst->s.data);

            // Allocate new memory. Round up to nearest 2K to minimize fragmentation
            dst->s.allocated = src->s.len | 0x7FF;
            dst->s.data = (char*) malloc(dst->s.allocated);

            if (!dst->s.data) {
                RMC_LOG_FATAL("Failed to allocate %u bytes of memory", dst->s.allocated);
                exit(255);
            }
        }
        memcpy(dst->s.data, src->s.data, src->s.len);
        dst->s.len = src->s.len;
        return 0;

    case vsd_int8:
    case vsd_uint8:
    case vsd_int16:
    case vsd_uint16:
    case vsd_int32:
    case vsd_uint32:
    case vsd_double:
    case vsd_float:
    case vsd_boolean:
        *dst = *src;
        return 0;

    default:
        RMC_LOG_FATAL("Cannot copy %s type data", vsd_data_type_to_string(data_type));
        exit(255);

    }
    return 0; // Not reached.
}




// Encode a signal tree under self.
static int encode_signal(vsd_desc_t* desc, uint8_t* buf, int buf_sz, int* len)
{
    vsd_desc_leaf_t* l_desc = 0;

    // Do we have enough space to encode signal ID?
    if (buf_sz < sizeof(desc->id))
        return ENOMEM;

    memcpy(buf, (void*) &(desc->id), sizeof(desc->id));
    buf += sizeof(desc->id);
    buf_sz -= sizeof(desc->id);
    *len += sizeof(desc->id);

    if (desc->elem_type == vsd_branch) {
        int rec_res = 0;
        vsd_desc_branch_t* br_desc = (vsd_desc_branch_t*) desc;

        vsd_desc_list_for_each(&br_desc->children,
                                  lambda(uint8_t,
                                         (vsd_desc_node_t* node, void* _ud) {
                                             int local_len = 0;
                                             // Abort recursion if we see an error.
                                             rec_res = encode_signal(node->data, buf, buf_sz, &local_len);
                                             if (rec_res != 0) {
                                                 *len += local_len;
                                                 buf_sz -= local_len;
                                                 return 0;
                                             }
                                             return 1;
                                         }), 0);

        // Return whatever the last result was in the recursion run.
        return rec_res;
    }

    // Encode a leaf node.
    switch(desc->data_type) {
    case vsd_int8:
    case vsd_uint8:
    case vsd_int16:
    case vsd_uint16:
    case vsd_int32:
    case vsd_uint32:
    case vsd_double:
    case vsd_float:
    case vsd_boolean:
        l_desc = (vsd_desc_leaf_t*) desc;
        // Do we have enough memory?
        if (buf_sz < _data_type_size[l_desc->base.data_type]) {
            RMC_LOG_ERROR("Could not encode %s signal ID %u. Needed %d bytes, %lu bytes available.",
                          vsd_data_type_to_string(l_desc->base.data_type),
                          l_desc->base.id,
                          _data_type_size[l_desc->base.data_type],
                          buf_sz);
            return ENOMEM;
        }

        // Copy out the raw data for the signal.
        memcpy(buf, (void*) &(l_desc->val), _data_type_size[l_desc->base.data_type]);
        buf += _data_type_size[l_desc->base.data_type];
        buf_sz -= _data_type_size[l_desc->base.data_type];
        *len += _data_type_size[l_desc->base.data_type];
        LM
        return 0;

    case vsd_string:
        l_desc = (vsd_desc_leaf_t*) desc;
        // Copy dynamic length string
        if (buf_sz < sizeof(uint32_t) + (l_desc->val.s.len)) {
            RMC_LOG_ERROR("Could not encode string signal ID %u. Needed %d bytes, %lu bytes available.",
                          l_desc->base.id,
                          sizeof(uint32_t) + (l_desc->val.s.len),
                          buf_sz);
            return ENOMEM;
        }
        // Copy string length
        memcpy(buf, (void*) &(l_desc->val.s.len), sizeof(l_desc->val.s.len));
        buf += sizeof(l_desc->val.s.len);
        buf_sz -= sizeof(l_desc->val.s.len);
        *len +=  sizeof(l_desc->val.s.len);

        // Copy string payload
        memcpy(buf, (void*) &(l_desc->val.s.data), l_desc->val.s.len);
        buf += l_desc->val.s.len;
        *len +=  l_desc->val.s.len;
        buf_sz -= l_desc->val.s.len;
        return 0;

    default:
        RMC_LOG_ERROR("Could not encode %s signal ID %u. Not supported",
                      vsd_data_type_to_string(l_desc->base.data_type),
                      desc->id);
        exit(255);
    }

    // Not reached.
    return 0;
}


// Decode incoming signal data and populate the local descriptor, and possibly
// the tree hanging under it (if it is a branch with children).
// The value will be stored in the signal descriptor tree hanging under
// context.
static int decode_signal(vsd_context_t* ctx, uint8_t* buf, int buf_sz, int *len, vsd_desc_t** desc)
{
    vsd_id_t id;
    int res;

    // Do we have enough data to decode signal ID?
    if (buf_sz < sizeof(id))
        return ENOMEM;

    // Record signal ID.
    id = *((vsd_id_t*) buf);
    buf += sizeof(id);
    buf_sz -= sizeof(id);
    *len += sizeof(id);

    // Locate signal in descriptor tree
    res = vsd_find_desc_by_id(ctx, id, desc);

    // Did we not find it?
    // This means that we have a signal definition mismatch between sender
    // and receiver.
    if (res) {
        RMC_LOG_ERROR("Cannot decode signal ID %u. Not defined: %s", id, strerror(res));
        return res;
    }

    // Is this a signal branch?
    if ((*desc)->elem_type == vsd_branch) {
        int rec_res = 0;
        vsd_desc_branch_t* br_desc = (vsd_desc_branch_t*) desc;
        vsd_desc_list_for_each(&br_desc->children,
                                  lambda(uint8_t,
                                         (vsd_desc_node_t* node, void* _ud) {
                                             int local_len = 0;
                                             vsd_desc_t *tmp_desc = 0;
                                             RMC_LOG_DEBUG("Decoding child with %d bytes left", local_len);
                                             // Abort recursion if we see an error.
                                             rec_res = decode_signal(ctx, buf, buf_sz, &local_len, &tmp_desc);

                                             if (rec_res != 0)
                                                 return 0;

                                             *len += local_len;
                                             buf_sz -= local_len;

                                             return 1;
                                         }), 0);

        // Return whatever the last result was in the recursion run.
        RMC_LOG_DEBUG("Got %s back", strerror(rec_res));
        return rec_res;
    }

    // Decode a leaf node.
    switch((*desc)->data_type) {
    case vsd_int8:
    case vsd_uint8:
    case vsd_int16:
    case vsd_uint16:
    case vsd_int32:
    case vsd_uint32:
    case vsd_double:
    case vsd_float:
    case vsd_boolean: {
        vsd_desc_leaf_t* l_desc = (vsd_desc_leaf_t*) *desc;

        // Do we have enough memory?
        if (buf_sz < _data_type_size[l_desc->base.data_type]) {
            RMC_LOG_ERROR("Could not decode %s signal ID %u. Needed %d bytes, %lu bytes available.",
                          vsd_data_type_to_string(l_desc->base.data_type),
                          l_desc->base.id,
                          _data_type_size[l_desc->base.data_type],
                          buf_sz);
            return ENOMEM;
        }

        // Copy out the raw data for the signal value
        l_desc->val= *((vsd_data_u*) buf);
        buf += _data_type_size[l_desc->base.data_type];
        buf_sz -= _data_type_size[l_desc->base.data_type];
        *len += _data_type_size[l_desc->base.data_type];
        return 0;
    }
    case vsd_string: {
        vsd_desc_leaf_t* l_desc = (vsd_desc_leaf_t*) desc;

       // Copy dynamic length string
        if (buf_sz < sizeof(uint32_t)) {
            RMC_LOG_ERROR("Could not decode string signal ID %u. Needed %d bytes, %lu bytes available.",
                          l_desc->base.id, sizeof(uint32_t), buf_sz);
            return ENOMEM;
        }

        // Grab length
        l_desc->val.s.len = *((uint32_t*) buf);
        buf += sizeof(l_desc->val.s.len);
        buf_sz -= sizeof(l_desc->val.s.len);
        *len += sizeof(l_desc->val.s.len);

        if (buf_sz < l_desc->val.s.len) {
            RMC_LOG_ERROR("Could not decode string signal ID %u. Needed %d bytes, %lu bytes available.",
                          l_desc->base.id, l_desc->val.s.len, buf_sz);
            return ENOMEM;
        }

        // Copy string payload
        vsd_data_copy(&l_desc->val, (vsd_data_u*) buf, vsd_string);
        buf += l_desc->val.s.len;
        buf_sz -= l_desc->val.s.len;
        len += l_desc->val.s.len;
        return 0;
    }

    default:
        RMC_LOG_ERROR("Could not decode %s signal ID %u. Not supported",
                      vsd_data_type_to_string((*desc)->data_type),
                      (*desc)->id);
        exit(255);
    }

    // Not reached.
    return 0;
}



// Set the context to use when we get inbound signals.
void vsd_set_active_context(vsd_context_t* ctx)
{
    _current_context = ctx;
}

int vsd_context_init(vsd_context_t* ctx)
{
    ctx->hash_table = 0; // Will be used by uthash.h macros.
    ctx->root = 0;
    // vsd_desc_create_branch(ctx, &ctx->root, "root", 0, "root branch", 0);
    return 0;
}

// ----------------------

int vsd_subscribe(vsd_context_t* ctx,
                          vsd_desc_t* desc,
                          vsd_subscriber_cb_t callback)
{
    vsd_subscriber_list_push_tail(&desc->subscribers, callback);
    return 0;
}

int vsd_unsubscribe(vsd_context_t* ctx,
                            vsd_desc_t* desc,
                            vsd_subscriber_cb_t callback)
{
    vsd_subscriber_node_t* node = 0;

    node = vsd_subscriber_list_find_node(&desc->subscribers, callback,
                                                 lambda(int, (vsd_subscriber_cb_t a,
                                                              vsd_subscriber_cb_t b) {
                                                            return a == b;
                                                        }));

    if (!node)
        return ESRCH; // No such subscriber.

    vsd_subscriber_list_delete(node);
    return 0;
}


// Send out all signals under desc as an atomic update
int vsd_publish(vsd_desc_t* desc)
{
    uint8_t buf[0xFF00];
    int len = 0;
    int res = 0;
    res = encode_signal(desc, buf, sizeof(buf), &len);
    if (res) {
        RMC_LOG_ERROR("Could not publish signal %lu: %s",
                      desc->id, strerror(res));
        return res;
    }

    RMC_LOG_INFO("Sending signal %s / %u: %d bytes payload", vsd_desc_to_path_static(desc), desc->id, len);

    return dstc_vsd_signal_transmit(desc->id, DYNAMIC_ARG(buf, len));
}


// Receive and deceode incoming signal, followed by invoking all callbacks.
// This function is invoked by DSTC as a result of a remote node
// calling vsd_transmit() through dstc_publish_signal() above.
void vsd_signal_transmit(vsd_id_t id, dstc_dynamic_data_t dynarg)
{
    int len = 0;
    int res = 0;
    vsd_desc_t* current = 0;
    vsd_desc_t* desc = 0;

    RMC_LOG_INFO("Got signal %u", id);
    if (!_current_context) {
        RMC_LOG_FATAL("Please call dstc_set_active_context() before receiving signals.");
        exit(255);
    }

    res = decode_signal(_current_context, dynarg.data, dynarg.length, &len, &desc);
    if (res) {
        RMC_LOG_ERROR("Could not decode incoming signal %lu: %s",
                      desc->id, strerror(res));
        return;
    }

    // Traverse signal desc tree upward and invoke subscribers.
    current = desc;
    while(current) {
        vsd_subscriber_list_for_each(&current->subscribers,
                                             lambda(uint8_t,
                                                    (vsd_subscriber_node_t* node, void* _ud) {
                                                        (*node->data)(desc);
                                                        return 1;
                                                    }), 0);
        current = &current->parent->base;
    }
}


vsd_data_type_e vsd_string_to_data_type(char* type)
{
    int ind = 0;

    for(ind = 0; ind < sizeof(_enum_data_type) / sizeof(_enum_data_type[0]); ind++)
        if (!strcasecmp(_enum_data_type[ind], type))
            return (vsd_data_type_e) ind;

    return vsd_na;
}


char* vsd_data_type_to_string(vsd_data_type_e type)
{
    if (sizeof(_enum_data_type) / sizeof(_enum_data_type[0]) <= type) {
        RMC_LOG_ERROR("Encountered unknownd data type enum: %d", type);
        return "*unknown*";
    }
    return _enum_data_type[type];
}



int vsd_desc_to_path(vsd_desc_t* desc, char* buf, int buf_len)
{
    vsd_desc_t* stack[256];
    int stp = 0;
    int tot_len = 0;

    if (!buf_len)
        return ENOMEM; // We need at least a null char.

    // No descriptor?
    if (!desc) {
        buf[0] = 0;
        return 0;
    }


    // We need to create the path name from the root descriptor to the leaf descriptor.
    // Traverse the tree from the lead (desc) upward toward the root and record
    // the path in stack.
    while(desc) {
        if (stp == sizeof(stack) / sizeof(stack[0]))
            return ENOMEM;

        stack[stp++] = desc;
        desc = &desc->parent->base;
    }

    // Traverse the stack in reverse, building up the path name
    // We have at least one element in stack.
    desc = stack[stp-1];

    while(stp--) {
        int len = 0;

        desc = stack[stp];
        len = strlen(desc->name);

        if (buf_len <= len + 1)
            return ENOMEM;

        memcpy(buf, desc->name, len);
        buf += len;
        tot_len += len;
        buf_len -= len;

        // Add a dot if we have more entries.
        if (stp > 0) {
            *buf = '.';
            buf++;
            tot_len++;
            buf_len--;
        }

    }
    buf[tot_len] = 0;
    return 0;
}

char* vsd_desc_to_path_static(vsd_desc_t* desc)
{
    static char res[1024];

    if (vsd_desc_to_path(desc, res, sizeof(res)) != 0)
        return "[signal path too long]";

    return res;
}

vsd_elem_type_e vsd_string_to_elem_type(char* type)
{
    int ind = 0;

    for(ind = 0; ind < sizeof(_enum_elem_type) / sizeof(_enum_elem_type[0]); ind++)
        if (!strcasecmp(_enum_elem_type[ind], type))
            return (vsd_elem_type_e) ind;

    return vsd_na;
}

char* vsd_elem_type_to_string(vsd_elem_type_e type)
{
    if (type >=  sizeof(_enum_elem_type) / sizeof(_enum_elem_type[0])) {
        RMC_LOG_ERROR("Encountered unknownd elem type enum: %d", type);
        return "*unknown*";
    }
    return _enum_elem_type[type];
}



int vsd_find_desc_by_id(vsd_context_t* context,
                               vsd_id_t id,
                               vsd_desc_t** result)
{
    if (!context || !result)
        return EINVAL;

    *result = 0;
    RMC_LOG_INFO("Finding signal %d", id);
    HASH_FIND_INT(context->hash_table, &id, *result);
    if (!*result) {
        RMC_LOG_WARNING("Could not find signal %u. %u elements", id, HASH_COUNT(context->hash_table));
        return ENOENT;
    }
    return 0;
}

int vsd_find_desc_by_path(vsd_context_t* context,
                          vsd_desc_t* root_desc,
                          char* path,
                          vsd_desc_t** result)
{
    char *path_separator = 0;
    vsd_desc_t* loc_res = 0;

    if (!context ||  !path || !result)
        return EINVAL;

    // IF no argument root and no context root, then we operate on an empty tree
    if (!root_desc && !context->root)
        return ENOENT;

    if (!root_desc)
        root_desc = &context->root->base;

    // Ensure that first element in root matches
    path_separator = strchr(path, '.');

    // If we found a path component separator, nil it out and
    // move to the next char after the separator
    // If no separator is found, path_separator == NULL, allowing
    // us to detect end of path
    if (strncmp(root_desc->name, path, path_separator?path_separator-path:strlen(path))) {
        RMC_LOG_DEBUG("Root element name %s is not start of path %s",
                     root_desc->name,
                     path);
        return ENOENT;
    }
    if (path_separator)
        path_separator++;

    path = path_separator;

    while(path) {

        path_separator = strchr(path, '.');
        int path_len = path_separator?path_separator-path:strlen(path);

        // We have to go deeper into the tree. Is our current
        // descriptor a branch that we can traverse into?
        if (root_desc->elem_type != vsd_branch) {
            RMC_LOG_WARNING("Component %s is not branch %s. Needs to be branch to travers",
                            root_desc->name,
                            vsd_elem_type_to_string(root_desc->data_type));
            return ENOTDIR;
        }

        RMC_LOG_DEBUG("Will search children of %s for %s", root_desc->name, path);
        loc_res = 0;
        vsd_desc_list_for_each(&((vsd_desc_branch_t*) root_desc)->children,
                                       lambda(uint8_t,
                                              (vsd_desc_node_t* node, void* _ud) {
                                                  RMC_LOG_DEBUG("Checking wanted %*s against child %s",
                                                                path_len, path, node->data->name);

                                                  if (!strncmp(path, node->data->name, path_len)) {
                                                      RMC_LOG_DEBUG("Got it");
                                                      loc_res = node->data;
                                                      return 0;
                                                  }
                                                  return 1; // Not find. Continue
                                              }), 0);
        if (!loc_res) {
            RMC_LOG_COMMENT("Child %*s not found under %s. ENOENT",
                            path_len, path, root_desc->name);

            return ENOENT;
        }
        root_desc = loc_res;
        // If we found a path component separator, nil it out and
        // move to the next char after the separator
        // If no separator is found, path_separator == NULL, allowing
        // us to detect end of path
        if (path_separator)
            path_separator++;


        path = path_separator;
        RMC_LOG_DEBUG("Moving into child %s.", root_desc->name);
    }
    RMC_LOG_DEBUG("Reached end of path with %s", root_desc->name);
    *result = root_desc;
    return 0;
}


int vsd_desc_init(vsd_context_t* ctx,
                         vsd_desc_t* desc,
                         vsd_elem_type_e elem_type,
                         vsd_data_type_e data_type,
                         vsd_desc_branch_t* parent,
                         vsd_id_t id,
                         char* name,
                         char* description)
{
    vsd_desc_t *result = 0;

    if (!ctx || !desc || !name || !description)
        return EINVAL;

    *desc = (vsd_desc_t) {
        .data_type = data_type,
        .elem_type = elem_type,
        .id = id,
        .name = strdup(name),
        .description = strdup(description),
        .parent = parent
    };


    if (!desc->name) {
        RMC_LOG_FATAL("Failed to malloc %d bytes: %s", strlen(name), strerror(errno));
        exit(255);
    }

    if (!desc->description) {
        RMC_LOG_FATAL("Failed to malloc %d bytes: %s", strlen(description), strerror(errno));
        exit(255);
    }

    if (parent)
        vsd_desc_list_push_tail(&parent->children, desc);

    vsd_subscriber_list_init(&desc->subscribers, 0, 0, 0);

    HASH_FIND_INT(ctx->hash_table, &id, result);
    if (result) {
        RMC_LOG_ERROR("Duplicate signal ID %u", id);
        exit(255);
    }

    HASH_ADD_INT(ctx->hash_table, id, desc);

    return 0;
}


// result->s is *not* owned by the caller. Use vsd_data_copy()
// if you need a copy.
int vsd_get_value(vsd_context_t* context,
                          vsd_id_t id,
                          vsd_data_u *result,
                          vsd_data_type_e* data_type)
{
    int res = 0;
    vsd_desc_t* desc = 0;

    res = vsd_find_desc_by_id(context, id, &desc);
    if (res)
        return res;

    if (desc->data_type == vsd_na ||
        desc->data_type == vsd_stream) {
        RMC_LOG_WARNING("Could not get value from type %s", vsd_data_type_to_string(desc->data_type));
        return EINVAL;
    }

    *result = ((vsd_desc_leaf_t*) desc)->val;
    *data_type = desc->data_type;
    return 0;
}


int vsd_desc_create_branch(vsd_context_t* ctx,
                                   vsd_desc_branch_t** res,
                                   char* name,
                                   vsd_id_t id,
                                   char* description,
                                   vsd_desc_branch_t* parent)
{

    if (!ctx || !res || !name || !description)
        return EINVAL;

    *res = (vsd_desc_branch_t*) malloc(sizeof(vsd_desc_branch_t));
    if (!*res) {
        RMC_LOG_FATAL("Failed to malloc %d bytes: %s", sizeof(vsd_desc_t), strerror(errno));
        exit(255);
    }

    vsd_desc_init(ctx,
                          &(*res)->base,
                          vsd_branch,
                          vsd_na,
                          parent,
                          id,
                          strdup(name),
                          strdup(description));

    (*res)->base.parent = parent;
    vsd_desc_list_init(&(*res)->children, 0, 0, 0);

    // Set root if parent is nil
    if (!parent) {
        // Check if root is alraedy set.
        if (ctx->root) {
            RMC_LOG_FATAL("Tried to set root twice. Old root: %s. New root: %s",
                          ctx->root->base.name, name);
            exit(255);
        }

        ctx->root = *res;
    }

    return 0;
}


int vsd_desc_create_leaf(vsd_context_t* ctx,
                                vsd_desc_leaf_t** res,
                                vsd_elem_type_e elem_type,
                                vsd_data_type_e data_type,
                                vsd_id_t id,
                                char* name,
                                char* description,
                                vsd_desc_branch_t* parent,
                                vsd_data_u min,
                                vsd_data_u max,
                                vsd_data_u val)
{
    if (!ctx || !res || !name || !description)
        return EINVAL;

    if (elem_type != vsd_attribute &&
        elem_type != vsd_sensor &&
        elem_type != vsd_actuator &&
        elem_type != vsd_element) {
        RMC_LOG_WARNING("Tried to create leaf with non leaf element type: %s", vsd_elem_type_to_string(elem_type));
        return EINVAL;
    }

    *res = (vsd_desc_leaf_t*) malloc(sizeof(vsd_desc_leaf_t));

    if (!*res) {
        RMC_LOG_FATAL("Failed to malloc %d bytes: %s", sizeof(vsd_desc_t), strerror(errno));
        exit(255);
    }

    vsd_desc_init(ctx,
                          &(*res)->base,
                          elem_type,
                          data_type,
                          parent,
                          id,
                          strdup(name),
                          strdup(description));

    // Copy in min/max/value
    vsd_data_copy(&(*res)->min, &min, data_type);
    vsd_data_copy(&(*res)->max, &max, data_type);
    vsd_data_copy(&(*res)->val, &val, data_type);
    return 0;
}


int vsd_desc_create_enum(vsd_context_t* ctx,
                                vsd_desc_enum_t** res,
                                vsd_elem_type_e elem_type,
                                vsd_data_type_e data_type,
                                vsd_id_t id,
                                char* name,
                                char* description,
                                vsd_desc_branch_t* parent,
                                vsd_data_u* enums, // Array of allowed values.
                                int enum_count)
{
    if (!ctx || !res || !name || !description)
        return EINVAL;


    if (elem_type != vsd_attribute &&
        elem_type != vsd_sensor &&
        elem_type != vsd_actuator &&
        elem_type != vsd_element) {
        RMC_LOG_WARNING("Tried to create enumerator with non leaf element type: %s", vsd_elem_type_to_string(elem_type));
        return EINVAL;
    }

    *res = (vsd_desc_enum_t*) malloc(sizeof(vsd_desc_enum_t));

    if (!*res) {
        RMC_LOG_FATAL("Failed to malloc %d bytes: %s", sizeof(vsd_desc_t), strerror(errno));
        exit(255);
    }

    vsd_desc_init(ctx, &(*res)->leaf.base, elem_type, data_type, parent, id, name, description);
    vsd_enum_list_init(&(*res)->enums, 0,0,0);

    // Copy in the enum pointers. We do not make a copy of them.
    while(enum_count--)
    {
        vsd_enum_list_push_tail(&(*res)->enums, *enums);
        ++enums;
    }
    return 0;
}


int vsd_desc_delete(vsd_context_t* context, vsd_desc_t* desc)
{
    // Check arguments
    if (!context || !desc)
        return EINVAL;

    free(desc->name);
    free(desc->description);
    if (desc->data_type == vsd_string) {
        // free(3) says that null pointer is ok.

        free(((vsd_desc_leaf_t*) desc)->val.s.data);
        free(((vsd_desc_leaf_t*) desc)->min.s.data);
        free(((vsd_desc_leaf_t*) desc)->max.s.data);

    }

    // Delete kids, if we have them.
    if (desc->elem_type == vsd_branch) {
        vsd_desc_branch_t* br_desc = (vsd_desc_branch_t*) desc;
        vsd_desc_t* child = 0;
        // Traverse all children and recurse.
        while(vsd_desc_list_pop_head(&br_desc->children, &child)) {
            vsd_desc_delete(context, child);
        }
    }
    free(desc);
    return 0;
}



vsd_desc_list_t* vsd_get_children(vsd_desc_t* parent)
{
    if (!parent || parent->elem_type != vsd_branch)
        return 0;

    return &((vsd_desc_branch_t*) parent)->children;
}


vsd_elem_type_e vsd_elem_type(vsd_desc_t* desc)
{
    if (!desc)
        return 0;

    return desc->elem_type;
}


vsd_data_type_e vsd_data_type(vsd_desc_t* desc)
{
    if (!desc)
        return 0;

    return desc->data_type;
}


char* vsd_name(vsd_desc_t* desc)
{
    if (!desc)
        return 0;

    return desc->name;
}


vsd_id_t vsd_id(vsd_desc_t* desc)
{
    if (!desc)
        return 0;

    return desc->id;
}


vsd_data_u vsd_value(vsd_desc_t* desc)
{
    if (!desc ||
        desc->data_type == vsd_stream ||
        desc->data_type == vsd_na)
        return vsd_data_u_nil;

    return ((vsd_desc_leaf_t*) desc)->val;
}

vsd_data_u vsd_min(vsd_desc_t* desc)
{
    if (!desc ||
        desc->data_type == vsd_stream ||
        desc->data_type == vsd_na)
        return vsd_data_u_nil;

    return ((vsd_desc_leaf_t*) desc)->min;
}

vsd_data_u vsd_max(vsd_desc_t* desc)
{
    if (!desc ||
        desc->data_type == vsd_stream ||
        desc->data_type == vsd_na)
        return vsd_data_u_nil;

    return ((vsd_desc_leaf_t*) desc)->max;
}


// --
// -- Set Value functions
// --

static int _find_and_validate_by_id(vsd_context_t* context,
                                    vsd_id_t id,
                                    vsd_data_type_e type,
                                    vsd_desc_t** desc)
{
    int res = 0;

    res = vsd_find_desc_by_id(context, id, desc);
    if (res)
        return res;

    if ((*desc)->elem_type == vsd_branch) {
        RMC_LOG_WARNING("Signal ID %u is a branch", id);
        return EISDIR;
    }

    if (type != vsd_na && (*desc)->data_type != type) {
        RMC_LOG_WARNING("Signal ID %u is %s, not boolean.",
                        id, vsd_data_type_to_string((*desc)->data_type));
        return EINVAL;
    }

    return 0;
}

static int _find_and_validate_by_desc(vsd_context_t* context,
                                      vsd_desc_t* desc,
                                      vsd_data_type_e type)
{
    if (desc->elem_type == vsd_branch) {
        RMC_LOG_WARNING("Signal ID %u is a branch", desc->id);
        return EISDIR;
    }

    if (type != vsd_na && desc->data_type != type) {
        RMC_LOG_WARNING("Signal ID %u is %s, not boolean.",
                        desc->id, vsd_data_type_to_string(desc->data_type));
        return EINVAL;
    }

    return 0;
}

static int _find_and_validate_by_path(vsd_context_t* context,
                                      char* path,
                                      vsd_data_type_e type,
                                      vsd_desc_t** desc)
{
    int res = 0;

    res = vsd_find_desc_by_path(context, 0, path, desc);
    if (res)
        return res;

    if ((*desc)->elem_type == vsd_branch) {
        RMC_LOG_WARNING("Signal ID %u is a branch", (*desc)->id);
        return EISDIR;
    }

    if (type != vsd_na && (*desc)->data_type != type) {
        RMC_LOG_WARNING("Signal ID %u is %s, not boolean.",
                        (*desc)->id, vsd_data_type_to_string((*desc)->data_type));
        return EINVAL;
    }
    return 0;
}

// -----
int vsd_set_value_by_desc_boolean(vsd_context_t* context, vsd_desc_t* desc, uint8_t val)
{
    int res = _find_and_validate_by_desc(context, desc, vsd_boolean);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.b = val;
    return 0;
}


int vsd_set_value_by_path_boolean(vsd_context_t* context, char* path, uint8_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_path(context, path, vsd_boolean, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.b = val;
    return 0;
}


int vsd_set_value_by_id_boolean(vsd_context_t* context, vsd_id_t id, uint8_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_id(context, id, vsd_boolean, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.b = val;

    return 0;
}


int vsd_set_value_by_desc_int8(vsd_context_t* context, vsd_desc_t* desc, int8_t val)
{
    int res = _find_and_validate_by_desc(context, desc, vsd_int8);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.i8 = val;
    return 0;
}

int vsd_set_value_by_path_int8(vsd_context_t* context, char* path, int8_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_path(context, path, vsd_int8, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.i8 = val;
    return 0;
}

int vsd_set_value_by_id_int8(vsd_context_t* context, vsd_id_t id, int8_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_id(context, id, vsd_int8, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.i8 = val;

    return 0;
}


int vsd_set_value_by_desc_uint8(vsd_context_t* context, vsd_desc_t* desc, uint8_t val)
{
    int res = _find_and_validate_by_desc(context, desc, vsd_uint8);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.u8 = val;
    return 0;
}

int vsd_set_value_by_path_uint8(vsd_context_t* context, char* path, uint8_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_path(context, path, vsd_uint8, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.u8 = val;
    return 0;
}

int vsd_set_value_by_id_uint8(vsd_context_t* context, vsd_id_t id, uint8_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_id(context, id, vsd_uint8, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.u8 = val;

    return 0;
}


int vsd_set_value_by_desc_int16(vsd_context_t* context, vsd_desc_t* desc, int16_t val)
{
    int res = _find_and_validate_by_desc(context, desc, vsd_int16);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.i16 = val;
    return 0;
}

int vsd_set_value_by_path_int16(vsd_context_t* context, char* path, int16_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_path(context, path, vsd_int16, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.i16 = val;
    return 0;
}

int vsd_set_value_by_id_int16(vsd_context_t* context, vsd_id_t id, int16_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_id(context, id, vsd_int16, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.i16 = val;

    return 0;
}


int vsd_set_value_by_desc_uint16(vsd_context_t* context, vsd_desc_t* desc, uint16_t val)
{
    int res = _find_and_validate_by_desc(context, desc, vsd_uint16);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.u16 = val;
    return 0;
}

int vsd_set_value_by_path_uint16(vsd_context_t* context, char* path, uint16_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_path(context, path, vsd_uint16, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.u16 = val;
    return 0;
}

int vsd_set_value_by_id_uint16(vsd_context_t* context, vsd_id_t id, uint16_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_id(context, id, vsd_uint16, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.u16 = val;

    return 0;
}


int vsd_set_value_by_desc_int32(vsd_context_t* context, vsd_desc_t* desc, int32_t val)
{
    int res = _find_and_validate_by_desc(context, desc, vsd_int32);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.i32 = val;
    return 0;
}

int vsd_set_value_by_path_int32(vsd_context_t* context, char* path, int32_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_path(context, path, vsd_int32, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.i32 = val;
    return 0;
}

int vsd_set_value_by_id_int32(vsd_context_t* context, vsd_id_t id, int32_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_id(context, id, vsd_int32, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.i32 = val;

    return 0;
}


int vsd_set_value_by_desc_uint32(vsd_context_t* context, vsd_desc_t* desc, uint32_t val)
{
    int res = _find_and_validate_by_desc(context, desc, vsd_uint32);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.u32 = val;
    return 0;
}

int vsd_set_value_by_path_uint32(vsd_context_t* context, char* path, uint32_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_path(context, path, vsd_uint32, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.u32 = val;
    return 0;
}

int vsd_set_value_by_id_uint32(vsd_context_t* context, vsd_id_t id, uint32_t val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_id(context, id, vsd_uint32, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.u32 = val;

    return 0;
}


int vsd_set_value_by_desc_float(vsd_context_t* context, vsd_desc_t* desc, float val)
{
    int res = _find_and_validate_by_desc(context, desc, vsd_float);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.f = val;
    return 0;
}

int vsd_set_value_by_path_float(vsd_context_t* context, char* path, float val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_path(context, path, vsd_float, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.f = val;
    return 0;
}

int vsd_set_value_by_id_float(vsd_context_t* context, vsd_id_t id, float val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_id(context, id, vsd_float, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.f = val;

    return 0;
}


int vsd_set_value_by_desc_double(vsd_context_t* context, vsd_desc_t* desc, double val)
{
    int res = _find_and_validate_by_desc(context, desc, vsd_float);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.f = val;
    return 0;
}

int vsd_set_value_by_path_double(vsd_context_t* context, char* path, double val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_path(context, path, vsd_double, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.d = val;
    return 0;
}

int vsd_set_value_by_id_double(vsd_context_t* context, vsd_id_t id, double val)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_id(context, id, vsd_double, &desc);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val.d = val;

    return 0;
}


int vsd_set_value_by_desc_string(vsd_context_t* context, vsd_desc_t* desc, char* data, int len)
{
    int res = _find_and_validate_by_desc(context, desc, vsd_string);
    vsd_data_u val;

    if (res)
        return res;


    res = vsd_data_copy(&((vsd_desc_leaf_t*) desc)->val,
                        &val,
                        desc->data_type);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val = val;
    return 0;
}

int vsd_set_value_by_path_string(vsd_context_t* context, char* path, char* data, int len)
{
    vsd_desc_t* desc = 0;
    vsd_data_u val;
    int res = _find_and_validate_by_path(context, path, vsd_string, &desc);

    if (res)
        return res;

    res = vsd_data_copy(&((vsd_desc_leaf_t*) desc)->val,
                        &val,
                        desc->data_type);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val = val;

    return 0;
}

int vsd_set_value_by_id_string(vsd_context_t* context, vsd_id_t id, char* data, int len)
{
    vsd_data_u val;
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_id(context, id, vsd_string, &desc);

    if (res)
        return res;

    if (res)
        return res;

    res = vsd_data_copy(&((vsd_desc_leaf_t*) desc)->val,
                        &val,
                        desc->data_type);

    if (res)
        return res;

    ((vsd_desc_leaf_t*) desc)->val = val;

    return 0;
}


int vsd_set_value_by_desc_convert(vsd_context_t* context, vsd_desc_t* desc, char* value)
{
    int res = _find_and_validate_by_desc(context, desc, vsd_na);
    vsd_data_u val;

    if (res)
        return res;

    res = vsd_string_to_data(desc->data_type, value, &val);
    if (res)
        return res;

    return vsd_data_copy(&((vsd_desc_leaf_t*) desc)->val,
                        &val,
                        desc->data_type);
}

int vsd_set_value_by_path_convert(vsd_context_t* context, char* path, char* value)
{
    vsd_desc_t* desc = 0;
    vsd_data_u val;
    int res = _find_and_validate_by_path(context, path, vsd_na, &desc);

    if (res)
        return res;

    res = vsd_string_to_data(desc->data_type, value, &val);

    if (res)
        return res;

    return vsd_data_copy(&((vsd_desc_leaf_t*) desc)->val,
                        &val,
                        desc->data_type);

    return 0;
}

int vsd_set_value_by_id_convert(vsd_context_t* context, vsd_id_t id, char* value)
{
    vsd_desc_t* desc = 0;
    int res = _find_and_validate_by_id(context, id, vsd_na, &desc);
    vsd_data_u val;

    if (res)
        return res;

    res = vsd_string_to_data(desc->data_type, value, &val);

    if (res)
        return res;

    return vsd_data_copy(&((vsd_desc_leaf_t*) desc)->val,
                        &val,
                        desc->data_type);

}
