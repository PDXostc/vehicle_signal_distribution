// Copyright (C) 2018, Jaguar Land Rover
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (mfeuer1@jaguarlandrover.com)
//

#ifndef __VEHICLE_SIGNAL_DISTRIBUTION_H__
#define __VEHICLE_SIGNAL_DISTRIBUTION_H__
#include <stdint.h>
#include <rmc_list.h>

// From
// github.com/GENIVI/vehicle_signal_specification/tree/master/tools/vspec2c
#include <vehicle_signal_specification.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union _vsd_data_u {
    int8_t i8;
    uint8_t u8;
    int16_t i16;
    uint16_t u16;
    int32_t i32;
    uint32_t u32;
    double d;
    float f;
    uint8_t b;
    struct {
        char* data;
        uint16_t len;
        uint16_t allocated;
    } s;
} vsd_data_u;

#define vsd_data_u_nil ({ vsd_data_u res = {0}; res; })

typedef struct vsd_context vsd_context_t;

RMC_LIST(vsd_signal_list, vsd_signal_node, struct _vss_signal_t*)
typedef vsd_signal_list vsd_signal_list_t;
typedef vsd_signal_node vsd_signal_node_t;
typedef void (*vsd_subscriber_cb_t)(vsd_context_t*, vsd_signal_list_t*);

RMC_LIST(vsd_subscriber_list, vsd_subscriber_node, vsd_subscriber_cb_t)
typedef vsd_subscriber_list vsd_subscriber_list_t;
typedef vsd_subscriber_node vsd_subscriber_node_t;

// Get the current value of a signal
extern int vsd_get_value(struct _vss_signal_t* sig,
                         vsd_data_u *result);

// Convert an arbitrary string to a vsd_data_u element.
extern int vsd_string_to_data(enum _vss_data_type_e type,
                              char* str,
                              vsd_data_u* res);

// Return the current signal's path as a static variable
// Do not modify the content of the returned string.
extern const char* vsd_signal_to_path_static(struct _vss_signal_t* sig);

// Set the boolean value of a signal identified by its pointer
//
// Sig is returned by vsd_find_signal_by_id() or vsd_find_signal_by_path().
//
// Return:
//  0 - Value set.
//  EISDIR - Sig points to a branch.
//  EINVAL - Sig is nil
//  EINVAL - Sig is not an uint8_t
//
// FIXME: Validate that val is legal if sig is enumerated.
// FIXME: Validate that val is not greater than maximum allowed value, if specified.
// FIXME: Validate that val is not less than minimum allowed value, if specified.

extern int vsd_set_value_by_signal_boolean(vsd_context_t* context, struct _vss_signal_t* sig, uint8_t val);

// Set the boolean value of a signal identified by its path.
// Path is the dot-separated path specified by the Vehicle Signal Specification
// project (Vehicle.Drivetrain.FuelSystem.TankCapacity).
//
// Return:
//  0 - Value set.
//  ENOENT - Signal with the given path name cannot be found.
//  EISDIR - Path refers to a branch.
//  ENOTDIR - One or more components in the path exist but are not branches.
//  EINVAL - Path is nil
//  EINVAL - Signal is not an uint8_t
extern int vsd_set_value_by_path_boolean(vsd_context_t* context, char* path, uint8_t val);

// Set the boolean value of a signal identified by its numerical signal ID.
// Signal ID is the unique ID assigned to the signal by the second field
// in the Vehicle Signal Specification CSV file loaded by vsd_load_from_file().
//
// Return:
//  0 - Value set.
//  ENOENT - Signal with the given ID cannot be found.
//  EISDIR - Path refers to a branch.
//  EINVAL - Signal is not an uint8_t
extern int vsd_set_value_by_index_boolean(vsd_context_t* context, int index, uint8_t val);

extern int vsd_set_value_by_signal_int8(vsd_context_t* context, struct _vss_signal_t* sig, int8_t val);
extern int vsd_set_value_by_path_int8(vsd_context_t* context, char* path, int8_t val);
extern int vsd_set_value_by_index_int8(vsd_context_t* context, int index, int8_t val);

extern int vsd_set_value_by_signal_uint8(vsd_context_t* context, struct _vss_signal_t* sig, uint8_t val);
extern int vsd_set_value_by_path_uint8(vsd_context_t* context, char* path, uint8_t val);
extern int vsd_set_value_by_index_uint8(vsd_context_t* context, int index, uint8_t val);

extern int vsd_set_value_by_signal_int16(vsd_context_t* context, struct _vss_signal_t* sig, int16_t val);
extern int vsd_set_value_by_path_int16(vsd_context_t* context, char* path, int16_t val);
extern int vsd_set_value_by_index_int16(vsd_context_t* context, int index, int16_t val);

extern int vsd_set_value_by_signal_uint16(vsd_context_t* context, struct _vss_signal_t* sig, uint16_t val);
extern int vsd_set_value_by_path_uint16(vsd_context_t* context, char* path, uint16_t val);
extern int vsd_set_value_by_index_uint16(vsd_context_t* context, int index, uint16_t val);

extern int vsd_set_value_by_signal_int32(vsd_context_t* context, struct _vss_signal_t* sig, int32_t val);
extern int vsd_set_value_by_path_int32(vsd_context_t* context, char* path, int32_t val);
extern int vsd_set_value_by_index_int32(vsd_context_t* context, int index, int32_t val);

extern int vsd_set_value_by_signal_uint32(vsd_context_t* context, struct _vss_signal_t* sig, uint32_t val);
extern int vsd_set_value_by_path_uint32(vsd_context_t* context, char* path, uint32_t val);
extern int vsd_set_value_by_index_uint32(vsd_context_t* context, int index, uint32_t val);

extern int vsd_set_value_by_signal_float(vsd_context_t* context, struct _vss_signal_t* sig, float val);
extern int vsd_set_value_by_path_float(vsd_context_t* context, char* path, float val);
extern int vsd_set_value_by_index_float(vsd_context_t* context, int index, float val);

extern int vsd_set_value_by_signal_double(vsd_context_t* context, struct _vss_signal_t* sig, double val);
extern int vsd_set_value_by_path_double(vsd_context_t* context, char* path, double val);
extern int vsd_set_value_by_index_double(vsd_context_t* context, int index, double val);

extern int vsd_set_value_by_signal_string(vsd_context_t* context, struct _vss_signal_t* sig, char* data);
extern int vsd_set_value_by_path_string(vsd_context_t* context, char* path, char* data);
extern int vsd_set_value_by_index_string(vsd_context_t* context, int index, char* data);

// Convert a literal string ("23.54") to the right type for the signal and
// set its value to it.
extern int vsd_set_value_by_signal_convert(vsd_context_t* context, struct _vss_signal_t* sig, char* value);
extern int vsd_set_value_by_path_convert(vsd_context_t* context, char* path, char* value);
extern int vsd_set_value_by_index_convert(vsd_context_t* context, int index, char* value);


// Install the full path to sig into a static buf and return it./
// If the path is longer than 1024 bytes, the string
// "[signal path too long]" will be returned.
extern const char* vsd_signal_to_path_static(struct _vss_signal_t* sig);

// Convert the provided string into a signal data element.
// The string is converted according to the type specified in 'type'.
// If conversion cannot be done, a default value is provided.
// Once this call returns a value installed in res, the result can
// be used to set the signal value via vsd_set_value().
//
// Return -
//  0 - OK
//  EINVAL - The data type in 'type' is not supported.
//
extern int vsd_string_to_data(enum _vss_data_type_e type,
                              char* str,
                              vsd_data_u* res);

// Publish the signal(s) in sig.
// If sig is a branch, all signals installed under it will be published atomically.
// Unchanged values will be published as well.
extern int vsd_publish(struct _vss_signal_t* sig);

// Subscribe to signal updates in sig
// If sig is a branch, any updates made to a signal under sig will be reported
// to the callback.
// If an unchanged value is received, it will still trigger a callback.
//
extern int vsd_subscribe(struct vsd_context* ctx,
                                 struct _vss_signal_t* sig,
                                 vsd_subscriber_cb_t callback);

// Unsubscribe to a signal previously subscribed to
extern int vsd_unsubscribe(struct vsd_context* ctx,
                                   struct _vss_signal_t* sig,
                                   vsd_subscriber_cb_t callback);


// Return the current value of sig.
extern vsd_data_u vsd_value(struct _vss_signal_t* sig);

// Return the minimum allowed value to the signal, if specified
// extern vsd_data_u vsd_min(struct _vss_signal_t* sig);

// Return the maximum allowed value to the signal, if specified
//extern vsd_data_u vsd_max(struct _vss_signal_t* sig);

#ifdef __cplusplus
}
#endif

#endif // __VEHICLE_SIGNAL_DISTRIBUTION_H__
