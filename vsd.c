// Copyright (C) 2018, Jaguar Land Rover
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (mfeuer1@jaguarlandrover.com)
//
// Common signal functions
//
#include <vehicle_signal_distribution.h>
#include <memory.h>
#include <string.h>
#include <errno.h>
#include <dstc.h>
#include <rmc_list_template.h>
#include <rmc_log.h>
#include "uthash.h"

DSTC_CLIENT(vsd_signal_transmit, uint32_t,, DSTC_DECL_DYNAMIC_ARG )
DSTC_SERVER(vsd_signal_transmit, uint32_t,, DSTC_DECL_DYNAMIC_ARG )

RMC_LIST_IMPL(vsd_signal_list, vsd_signal_node, vss_signal_t*)
RMC_LIST_IMPL(vsd_subscriber_list, vsd_subscriber_node, vsd_subscriber_cb_t)


typedef struct _vsd_user_data_t {
    vsd_data_u value;
    vsd_subscriber_list_t subscribers;
} vsd_user_data_t;

static void* _user_data = 0;

static int _data_type_size[] =
{
    sizeof(int8_t),    // VSS_INT8
    sizeof(uint8_t),   // VSS_UINT8
    sizeof(int16_t),   // VSS_INT16
    sizeof(uint16_t),  // VSS_UINT16
    sizeof(int32_t),   // VSS_INT32
    sizeof(uint32_t),  // VSS_UINT32
    sizeof(float),     // VSS_FLOAT
    sizeof(double),    // VSS_DOUBLE
    1,                 // VSS_BOOLEAN
    -1,                // VSS_STRING
    -1,                // VSS_STREAM
    -1,                // VSS_NA
};

// Simple UT hash table to map signatures
// to signals.
// https://troydhanson.github.io/uthash/userguide.html

// Populated by get_signal_by_signature() when
// no hits were found in the hash table.
//
typedef struct {
    uint32_t signature;
    vss_signal_t* signal;
    UT_hash_handle hh;
} signal_hash_t;

static signal_hash_t* _signatures = NULL;

static int vsd_data_copy(vsd_data_u* dst,
                         vsd_data_u* src,
                         vss_data_type_e data_type)
{
    switch(data_type) {
    case VSS_STRING:
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

    case VSS_INT8:
    case VSS_UINT8:
    case VSS_INT16:
    case VSS_UINT16:
    case VSS_INT32:
    case VSS_UINT32:
    case VSS_DOUBLE:
    case VSS_FLOAT:
    case VSS_BOOLEAN:
        *dst = *src;
        return 0;

    default:
        RMC_LOG_FATAL("Cannot copy %s type data", vss_data_type_string(data_type));
        exit(255);

    }
    return 0; // Not reached.
}


int vsd_string_to_data(vss_data_type_e type, char* str, vsd_data_u* res)
{
    switch(type) {
    case VSS_INT8: { *res = (vsd_data_u) { .i8 = (int8_t) strtol(str, 0, 10) }; return 0; }
    case VSS_UINT8: { *res = (vsd_data_u) { .u8 = (uint8_t) strtoul(str, 0, 10) }; return 0; }
    case VSS_INT16: { *res = (vsd_data_u) { .i16 = (int16_t) strtol(str, 0, 10) }; return 0; }
    case VSS_UINT16: { *res = (vsd_data_u) { .u16 = (uint16_t) strtoul(str, 0, 10) }; return 0; }
    case VSS_INT32: { *res = (vsd_data_u) { .i32 = (int32_t) strtol(str, 0, 10) }; return 0; }
    case VSS_UINT32: { *res = (vsd_data_u) { .u32 = (uint32_t) strtoul(str, 0, 10) }; return 0; }
    case VSS_DOUBLE: { *res = (vsd_data_u) { .d = (double) strtod(str, 0) }; return 0; }
    case VSS_FLOAT: { *res = (vsd_data_u) { .f = (float) strtof(str, 0) }; return 0; }
    case VSS_BOOLEAN: { *res = (vsd_data_u) { .b = (*str == '1' || *str == 't' || *str == 'T')?1:0 }; return 0; }
    case VSS_STRING: { *res = (vsd_data_u) { .s.data = str, .s.len = strlen(str)+1 }; return 0; }
    default:
        RMC_LOG_WARNING("Illegal type: %d / %s\n", type, str);
        *res = vsd_data_u_nil;
        return EINVAL;
    }
}

static vsd_user_data_t* vsd_user_data(vss_signal_t* sig)
{
    // We malloc data for the signal, if not already done.  Since we
    // never free the data we don't run into memory fragmentation
    // issues.
    if (!sig->user_data) {
        sig->user_data = (void*) malloc(sizeof(vsd_user_data_t));
        if (!sig->user_data) {
            RMC_LOG_FATAL("Failed to allocate %ld bytes.", sizeof(vsd_user_data_t));
            exit(255);
        }
        memset(sig->user_data, 0, sizeof(vsd_user_data_t));
        vsd_subscriber_list_init(&((vsd_user_data_t*) sig->user_data)->subscribers, 0, 0, 0);
    }
    return (vsd_user_data_t*) sig->user_data;
}

static vsd_subscriber_list_t* vsd_subscribers(vss_signal_t* sig)
{
    vsd_user_data_t* dt = vsd_user_data(sig);

    return &dt->subscribers;
}

static vsd_data_u* vsd_data(vss_signal_t* sig)
{
    vsd_user_data_t* dt = vsd_user_data(sig);

    return &dt->value;
}

static vss_signal_t* _get_signal_by_signature(uint32_t signature)
{
    signal_hash_t* hash = 0;
    int ind = vss_get_signal_count();
    vss_signal_t* signal = 0;
    extern vss_signal_t vss_signal[];

    // Is it already in the hash table?
    HASH_FIND_INT(_signatures, &signature, hash);
    if (hash)
        return hash->signal;

    // Add a new hash.
    // First do a linear search.
    while(ind--) {
        signal = vss_get_signal_by_index(ind);
        if (vss_get_subtree_signature(signal) == signature)
            break;
    }
    // No hit?
    if (ind == -1)
        return 0;

    // Our linear search found a signal. Add it to the hash table
    hash = (signal_hash_t*) malloc(sizeof(signal_hash_t));
    if (!hash) {
        RMC_LOG_FATAL("Could not allocate %d bytes", sizeof(signal_hash_t));
        exit(255);
    }
    RMC_LOG_DEBUG("_get_signal_by_signature(0x%X): Adding signal to hash table for quicker lookup",
                  signature);
    hash->signal = signal;
    hash->signature = signature;
    HASH_ADD_INT(_signatures, signature, hash);
    return signal;
}

// Encode a signal tree under self.
static int encode_signal(vss_signal_t* sig, uint8_t* buf, int buf_sz, int* len)
{
    const vsd_data_u *sig_val = 0;
    *len = 0;
    if (sig->element_type == VSS_BRANCH) {
        int ind = 0;
        int rec_res = 0;
        while(sig->children[ind]) {
            int local_len = 0;
            // Abort recursion if we see an error.
            rec_res = encode_signal(sig->children[ind], buf, buf_sz, &local_len);
            if (rec_res != 0) {
                RMC_LOG_WARNING("Failed to encode %s: %s",
                                sig->children[ind]->name,
                                strerror(rec_res));
                break;
            }
            *len += local_len;
            buf_sz -= local_len;
            buf += local_len;
            ind++;
        }

        // Return whatever the last result was in the recursion run.
        return rec_res;
    }

    // We are not a branch, but an actual signal
    // Do we have enough space to encode signal ID?
    if (buf_sz < sizeof(sig->signature))
        return ENOMEM;

    memcpy(buf, (void*) &(sig->signature), sizeof(sig->signature));
    buf += sizeof(sig->signature);
    buf_sz -= sizeof(sig->signature);
    *len += sizeof(sig->signature);
    sig_val = vsd_data(sig);

    // Encode a leaf node.
    switch(sig->data_type) {
    case VSS_INT8:
    case VSS_UINT8:
    case VSS_INT16:
    case VSS_UINT16:
    case VSS_INT32:
    case VSS_UINT32:
    case VSS_DOUBLE:
    case VSS_FLOAT:
    case VSS_BOOLEAN:
        // Do we have enough memory?
        if (buf_sz < _data_type_size[sig->data_type]) {
            RMC_LOG_ERROR("Could not encode %s signal %u. Needed %d bytes, %lu bytes available.",
                          vss_data_type_string(sig->data_type),
                          sig->uuid,
                          _data_type_size[sig->data_type],
                          buf_sz);
            return ENOMEM;
        }

        RMC_LOG_DEBUG("Encoding %s/%s %s/%d. %d bytes",
                      sig->name, sig->uuid,
                      vss_data_type_string(sig->data_type),
                      sig->data_type,
                      _data_type_size[sig->data_type]);

        // Copy out the raw data for the signal.

        memcpy(buf, (void*) sig_val, _data_type_size[sig->data_type]);
        buf += _data_type_size[sig->data_type];
        buf_sz -= _data_type_size[sig->data_type];
        *len += _data_type_size[sig->data_type];
        return 0;

    case VSS_STRING:
        // Copy dynamic length string
        if (buf_sz < sizeof(uint32_t) + sig_val->s.len) {
            RMC_LOG_ERROR("Could not encode string signal ID %s. Needed %d bytes, %lu bytes available.",
                          sig->uuid,
                          sizeof(uint32_t) + sig_val->s.len,
                          buf_sz);
            return ENOMEM;
        }
        // Copy string length
        memcpy(buf, (void*) &sig_val->s.len, sizeof(sig_val->s.len));
        buf += sizeof(sig_val->s.len);
        buf_sz -= sizeof(sig_val->s.len);
        *len +=  sizeof(sig_val->s.len);
        RMC_LOG_DEBUG("Encoding %s/%s string length as %d bytes",
                      sig->name, sig->uuid, sizeof(sig_val->s.len));

        // Copy string payload
        memcpy(buf, (void*) sig_val->s.data, sig_val->s.len);
        buf += sig_val->s.len;
        *len +=  sig_val->s.len;
        buf_sz -= sig_val->s.len;
        RMC_LOG_DEBUG("String is %d bytes",
                      vsd_data(sig)->s.len);
        return 0;

    default:
        RMC_LOG_ERROR("Could not encode %s signal %s. Not supported",
                      vss_data_type_string(sig->data_type),
                      sig->uuid);
        exit(255);
    }

    // Not reached.
    return 0;
}


// Decode incoming signal data and populate the local signal, and possibly
// the tree hanging under it (if it is a branch with children).
// The value will be stored in the signal tree hanging under
// context.
static int decode_signal(vsd_context_t* ctx,
                         const uint8_t* buf, int buf_sz,
                         vsd_signal_list_t* res_lst)
{
    uint32_t signature;
    vss_signal_t* sig = 0;

    while(buf_sz) {
        // Do we have enough data to decode signal signature?
        if (buf_sz < sizeof(signature))
            return ENOMEM;

        // Extract signal signature.
        signature = *((uint32_t*) buf);
        buf += sizeof(signature);
        buf_sz -= sizeof(signature);

        // Locate signal in tree
        sig = _get_signal_by_signature(signature);

        // If not found then we have a signal definition mismatch between sender
        // and receiver.
        if (!sig) {
            RMC_LOG_FATAL("Cannot decode signal signature 0x%X.", signature);
            exit(255);
        }

        // Is this a signal branch?
        if (sig->element_type == VSS_BRANCH) {
            RMC_LOG_FATAL("Received a branch as a signal. signature 0x%X", signature);
            exit(255);
        }

        // Decode a leaf node.
        switch(sig->data_type) {
        case VSS_INT8:
        case VSS_UINT8:
        case VSS_INT16:
        case VSS_UINT16:
        case VSS_INT32:
        case VSS_UINT32:
        case VSS_DOUBLE:
        case VSS_FLOAT:
        case VSS_BOOLEAN: {

            // Do we have enough memory?
            if (buf_sz < _data_type_size[sig->data_type]) {
                RMC_LOG_ERROR("Could not decode %s signal signature 0x%X. Needed %d bytes, %lu bytes available.",
                              vss_data_type_string(sig->data_type),
                              sig->signature,
                              _data_type_size[sig->data_type],
                              buf_sz);
                return ENOMEM;
            }

            // Copy out the raw data for the signal value
            *vsd_data(sig) = *((vsd_data_u*) buf);
            buf += _data_type_size[sig->data_type];
            buf_sz -= _data_type_size[sig->data_type];

            vsd_signal_list_push_tail(res_lst, sig);
            break;
        }

        case VSS_STRING: {
            vsd_data_u val;

            // Copy dynamic length string
            if (buf_sz < sizeof(uint16_t)) {
                RMC_LOG_ERROR("Could not decode string length of signal %s. Needed %d bytes, %lu bytes available.",
                              sig->uuid, sizeof(uint32_t), buf_sz);
                return ENOMEM;
            }

            // Grab length
            val.s.len = *((uint16_t*) buf);
            val.s.allocated = 0;
            buf += sizeof(val.s.len);
            buf_sz -= sizeof(val.s.len);
            val.s.data = (char*) buf;
            if (buf_sz < val.s.len) {
                RMC_LOG_ERROR("Could not decode string signal %s. Needed %d bytes, %lu bytes available.",
                              sig->uuid, vsd_data(sig)->s.len, buf_sz);
                return ENOMEM;
            }

            // Copy string payload
            vsd_data_copy(vsd_data(sig), &val, VSS_STRING);
            buf += val.s.len;
            buf_sz -= val.s.len;

            vsd_signal_list_push_tail(res_lst, sig);
            break;
        }

        default:
            RMC_LOG_ERROR("Could not decode %s signal signature 0x%X. Not supported",
                          vss_data_type_string(sig->data_type),
                          sig->signature);
            exit(255);
        }
    }

    return 0;
}

int vsd_set_user_data(vsd_context_t* ctx, void* user_data)
{

    _user_data = user_data;
    return 0;
}

void* vsd_get_user_data(vsd_context_t* ctx)
{
    return _user_data;
}


// ----------------------

int vsd_subscribe(vsd_context_t* ctx,
                  vss_signal_t* sig,
                  vsd_subscriber_cb_t callback)
{
    vsd_subscriber_list_push_tail(vsd_subscribers(sig), callback);
    return 0;
}

static int _subscriber_compare(vsd_subscriber_cb_t a,
                               vsd_subscriber_cb_t b,
                               void* user_data)
{
    return a == b;
}

int vsd_unsubscribe(vsd_context_t* ctx,
                    vss_signal_t* sig,
                    vsd_subscriber_cb_t callback)
{
    vsd_subscriber_node_t* node = 0;

    node = vsd_subscriber_list_find_node(vsd_subscribers(sig),
                                         callback,
                                         _subscriber_compare, 0);

    if (!node)
        return ESRCH; // No such subscriber.

    vsd_subscriber_list_delete(node);
    return 0;
}


// Send out all signals under sig as an atomic update
int vsd_publish(vss_signal_t* sig)
{
    uint8_t buf[0xFF00];
    int len = 0;
    int res = 0;

    res = encode_signal(sig, buf, sizeof(buf), &len);
    if (res) {
        RMC_LOG_ERROR("Could not publish signal %s: %s",
                      sig->uuid, strerror(res));
        return res;
    }

    RMC_LOG_INFO("Sending signal%s: %d bytes payload",
                 sig->uuid, len);

    // Use the four first bytes of the subtree signature for the signal (or signal tree)
    // we are transmitting. If the receiver's corresponding signautre
    // does not match it means that the specs used for the subtree differ between
    // the pubhlisher and the receiver.
    return dstc_vsd_signal_transmit(sig->signature, DSTC_DYNAMIC_ARG(buf, len));
}


static uint8_t _invoke_subscriber(vsd_subscriber_node_t* node, void* user_data)
{
    vsd_signal_list_t *res_lst = (vsd_signal_list_t*) user_data;

    (*node->data)(0, res_lst);
    return 1;
}




// Receive and deceode incoming signal, followed by invoking all callbacks.
// This function is invoked by DSTC as a result of a remote node
// calling vsd_transmit() through dstc_publish_signal() above.
void vsd_signal_transmit(uint32_t vss_signature, dstc_dynamic_data_t dynarg)
{
    int res = 0;
    vss_signal_t* current = 0;
    vss_signal_t* sig = 0;
    vsd_signal_list_t res_lst;

    sig = _get_signal_by_signature(vss_signature);
    if (!sig) {
        RMC_LOG_ERROR("Could not resovle index %d to a signal\n",
                      sig, strerror(res));
        return;
    }


    // Check that we have a signature match on the given node in the tree.
    // We use only the first four bytes of the sha256 code since
    // that will very, very likely be enough to detect signal spec
    // mismatch.
    if (sig->signature != vss_signature) {
        RMC_LOG_FATAL("VSS signature mismatch. My signature: 0x%X. Their signature: 0x%X",
                      sig->signature, vss_signature);
        RMC_LOG_FATAL("Offending signal UUID: %s\n", sig->uuid);
        exit(255);
    }

    vsd_signal_list_init(&res_lst, 0, 0, 0);

    res = decode_signal(0, dynarg.data, dynarg.length, &res_lst);

    if (res) {
        RMC_LOG_ERROR("Could not decode incoming signal %s tree: %s",
                      sig->uuid, strerror(res));
        return;
    }

    // Traverse signal tree, as provided in the root id argument,
    // upward and invoke subscribers.
    current = sig;
    while(current) {
        vsd_subscriber_list_for_each(vsd_subscribers(current),
                                     _invoke_subscriber, &res_lst);
        current = current->parent;
    }
    vsd_signal_list_empty(&res_lst);
}



const char* vsd_signal_to_path_static(vss_signal_t* sig)
{
//    static char res[1024];

    // FIXME: vss_get_signal_path_is_missing from libvss.so
//    if (vss_get_signal_path(sig, res, sizeof(res)) != 0)
//        return "[signal path too long]";

//    return res;
    return "not implemented";
}


// result->s is *not* owned by the caller. Use vss_data_copy()
// if you need a copy.
int vsd_get_value(vss_signal_t* sig,
                  vsd_data_u *result)
{
    if (sig->data_type == VSS_NA ||
        sig->data_type == VSS_STREAM) {
        RMC_LOG_WARNING("Could not get value from type %s", vss_data_type_string(sig->data_type));
        return EINVAL;
    }

    *result = *vsd_data(sig);
    return 0;
}



// -----
int vsd_set_value_by_signal_boolean(vsd_context_t* context, vss_signal_t* sig, uint8_t val)
{
    vsd_data(sig)->b = val;
    return 0;
}


int vsd_set_value_by_path_boolean(vsd_context_t* context, char* path, uint8_t val)
{
    vss_signal_t*  sig  = 0;
    int res =vss_get_signal_by_path(path, &sig);

    if (res)
        return res;

    vsd_data(sig)->b = val;
    return 0;
}


int vsd_set_value_by_index_boolean(vsd_context_t* context, int index, uint8_t val)
{
    vss_signal_t* sig = vss_get_signal_by_index(index);

    if (!sig)
        return ENOENT;

    vsd_data(sig)->b = val;

    return 0;
}


int vsd_set_value_by_signal_int8(vsd_context_t* context, vss_signal_t* sig, int8_t val)
{
    vsd_data(sig)->i8 = val;
    return 0;
}

int vsd_set_value_by_path_int8(vsd_context_t* context, char* path, int8_t val)
{
    vss_signal_t*  sig = 0;
    int res = vss_get_signal_by_path(path, &sig);

    if (res)
        return res;

    vsd_data(sig)->i8 = val;
    return 0;
}

int vsd_set_value_by_index_int8(vsd_context_t* context, int index, int8_t val)
{
    vss_signal_t* sig = vss_get_signal_by_index(index);

    if (!sig)
        return ENOENT;

    vsd_data(sig)->i8 = val;

    return 0;
}

int vsd_set_value_by_signal_uint8(vsd_context_t* context, vss_signal_t* sig, uint8_t val)
{
    vsd_data(sig)->u8 = val;
    return 0;
}

int vsd_set_value_by_path_uint8(vsd_context_t* context, char* path, uint8_t val)
{
    vss_signal_t*  sig = 0;
    int res = vss_get_signal_by_path(path, &sig);

    if (res)
        return res;

    vsd_data(sig)->u8 = val;
    return 0;
}

int vsd_set_value_by_index_uint8(vsd_context_t* context, int index, uint8_t val)
{
    vss_signal_t* sig = vss_get_signal_by_index(index);

    if (!sig)
        return ENOENT;

    vsd_data(sig)->u8 = val;

    return 0;
}


int vsd_set_value_by_signal_int16(vsd_context_t* context, vss_signal_t* sig, int16_t val)
{
    vsd_data(sig)->i16 = val;
    return 0;
}

int vsd_set_value_by_path_int16(vsd_context_t* context, char* path, int16_t val)
{
    vss_signal_t*  sig = 0;
    int res = vss_get_signal_by_path(path, &sig);

    if (res)
        return res;

    vsd_data(sig)->i16 = val;
    return 0;
}

int vsd_set_value_by_index_int16(vsd_context_t* context, int index, int16_t val)
{
    vss_signal_t* sig = vss_get_signal_by_index(index);

    if (!sig)
        return ENOENT;

    vsd_data(sig)->i16 = val;

    return 0;
}


int vsd_set_value_by_signal_uint16(vsd_context_t* context, vss_signal_t* sig, uint16_t val)
{
    vsd_data(sig)->u16 = val;
    return 0;
}

int vsd_set_value_by_path_uint16(vsd_context_t* context, char* path, uint16_t val)
{
    vss_signal_t*  sig = 0;
    int res = vss_get_signal_by_path(path, &sig);

    if (res)
        return res;

    vsd_data(sig)->u16 = val;
    return 0;
}

int vsd_set_value_by_index_uint16(vsd_context_t* context, int index, uint16_t val)
{
    vss_signal_t* sig = vss_get_signal_by_index(index);

    if (!sig)
        return ENOENT;

    vsd_data(sig)->u16 = val;

    return 0;
}


int vsd_set_value_by_signal_int32(vsd_context_t* context, vss_signal_t* sig, int32_t val)
{
    vsd_data(sig)->i32 = val;
    return 0;
}

int vsd_set_value_by_path_int32(vsd_context_t* context, char* path, int32_t val)
{
    vss_signal_t*  sig = 0;
    int res = vss_get_signal_by_path(path, &sig);

    if (res)
        return res;

    vsd_data(sig)->i32 = val;
    return 0;
}

int vsd_set_value_by_index_int32(vsd_context_t* context, int index, int32_t val)
{
    vss_signal_t* sig = vss_get_signal_by_index(index);

    if (!sig)
        return ENOENT;

    vsd_data(sig)->i32 = val;

    return 0;
}


int vsd_set_value_by_signal_uint32(vsd_context_t* context, vss_signal_t* sig, uint32_t val)
{
    vsd_data(sig)->u32 = val;
    return 0;
}

int vsd_set_value_by_path_uint32(vsd_context_t* context, char* path, uint32_t val)
{
    vss_signal_t*  sig = 0;
    int res = vss_get_signal_by_path(path, &sig);

    if (res)
        return res;

    vsd_data(sig)->u32 = val;
    return 0;
}

int vsd_set_value_by_index_uint32(vsd_context_t* context, int index, uint32_t val)
{
    vss_signal_t* sig = vss_get_signal_by_index(index);

    if (!sig)
        return ENOENT;

    vsd_data(sig)->u32 = val;

    return 0;
}


int vsd_set_value_by_signal_float(vsd_context_t* context, vss_signal_t* sig, float val)
{
    vsd_data(sig)->f = val;
    return 0;
}

int vsd_set_value_by_path_float(vsd_context_t* context, char* path, float val)
{
    vss_signal_t*  sig = 0;
    int res = vss_get_signal_by_path(path, &sig);

    if (res)
        return res;

    vsd_data(sig)->f = val;
    return 0;
}

int vsd_set_value_by_index_float(vsd_context_t* context, int index, float val)
{
    vss_signal_t* sig = vss_get_signal_by_index(index);

    if (!sig)
        return ENOENT;

    vsd_data(sig)->f = val;

    return 0;
}


int vsd_set_value_by_signal_double(vsd_context_t* context, vss_signal_t* sig, double val)
{
    vsd_data(sig)->d = val;
    return 0;
}

int vsd_set_value_by_path_double(vsd_context_t* context, char* path, double val)
{
    vss_signal_t*  sig = 0;
    int res = vss_get_signal_by_path(path, &sig);

    if (res)
        return res;

    vsd_data(sig)->d = val;
    return 0;
}

int vsd_set_value_by_index_double(vsd_context_t* context, int index, double val)
{
    vss_signal_t* sig = vss_get_signal_by_index(index);

    if (!sig)
        return ENOENT;

    vsd_data(sig)->d = val;

    return 0;
}


int vsd_set_value_by_signal_string(vsd_context_t* context, vss_signal_t* sig, char* data)
{
    int res = 0;
    vsd_data_u val;

    res = vsd_string_to_data(sig->data_type, data, &val);

    if (res)
        return res;

    return vsd_data_copy(vsd_data(sig),
                         &val,
                         sig->data_type);

}

int vsd_set_value_by_path_string(vsd_context_t* context, char* path, char* data)
{
    vss_signal_t*  sig = 0;
    int res = vss_get_signal_by_path(path, &sig);
    vsd_data_u val;

    if (res)
        return res;

    res = vsd_string_to_data(sig->data_type, data, &val);

    if (res)
        return res;

    *vsd_data(sig) = val;

    return 0;
}

int vsd_set_value_by_index_string(vsd_context_t* context, int index, char* data)
{
    vsd_data_u val;

    vss_signal_t* sig = vss_get_signal_by_index(index);
    int res = 0;

    res = vsd_string_to_data(sig->data_type, data, &val);
    if (res)
        return res;


    if (res)
        return res;

    *vsd_data(sig) = val;

    return 0;
}



int vsd_set_value_by_signal_convert(vsd_context_t* context, vss_signal_t* sig, char* value)
{
    vsd_data_u val;
    int res;

    res = vsd_string_to_data(sig->data_type, value, &val);
    if (res)
        return res;

    return vsd_data_copy(vsd_data(sig), &val, sig->data_type);
}

int vsd_set_value_by_path_convert(vsd_context_t* context, char* path, char* value)
{
    vss_signal_t* sig = 0;
    int res = vss_get_signal_by_path(path, &sig);
    vsd_data_u val;

    if (res)
        return res;

    res = vsd_string_to_data(sig->data_type, value, &val);

    if (res)
        return res;

    return vsd_data_copy(vsd_data(sig), &val, sig->data_type);
}

int vsd_set_value_by_index_convert(vsd_context_t* context, int index, char* value)
{
    vss_signal_t* sig = vss_get_signal_by_index(index);
    vsd_data_u val;
    int res;

    if (!sig)
        return ENOENT;

    res = vsd_string_to_data(sig->data_type, value, &val);

    if (res)
        return res;

    return vsd_data_copy(vsd_data(sig), &val, sig->data_type);
}
