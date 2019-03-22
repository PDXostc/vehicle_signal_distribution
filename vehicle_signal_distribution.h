// Copyright (C) 2018, Jaguar Land Rover
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (mfeuer1@jaguarlandrover.com)
//

#include <stdint.h>
#include <rmc_list.h>

typedef enum {
    vsd_int8 = 0,
    vsd_uint8 = 1,
    vsd_int16 = 2,
    vsd_uint16 = 3,
    vsd_int32 = 4,
    vsd_uint32 = 5,
    vsd_double = 6,
    vsd_float = 7,
    vsd_boolean = 8,
    vsd_string = 9,
    vsd_stream = 10,
    vsd_na = 11,
} vsd_data_type_e;

typedef enum {
    vsd_attribute = 0,
    vsd_branch = 1,
    vsd_sensor = 2,
    vsd_actuator = 3,
    vsd_rbranch = 4,
    vsd_element = 5,
} vsd_elem_type_e;

typedef union {
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

typedef struct vsd_context vsd_context_t;
typedef struct vsd_desc vsd_desc_t;
typedef struct vsd_desc_branch vsd_desc_branch_t;
typedef uint32_t vsd_id_t;


RMC_LIST(vsd_desc_list, vsd_desc_node, struct vsd_desc*)
typedef vsd_desc_list vsd_desc_list_t;
typedef vsd_desc_node vsd_desc_node_t;

typedef void (*vsd_subscriber_cb_t)(vsd_desc_list_t* );

// Find a signal descriptor by its path name
// "Vehicle.Drivetrain.FuelSystem.TankCapacity" If parent_desc != 0,
// then search from that descriptor downard.  If parent_desc == 0,
// then use context->root as the starting point *result is set to the
// found descriptor, or 0 if not found
//
// Return
//  0 - Descriptor returnedc
//  ENOENT - One or more components in the path are missing.
//  ENODIR - One or more components in the path are not branches.
extern int vsd_find_desc_by_path(struct vsd_context* context,
                                         struct vsd_desc* parent_desc,
                                         char* path,
                                         struct vsd_desc** result);

// Find a signal descriptor by its signal ID.
//
// Return
//  0 - Descriptor returnedc
//  ENOENT - One or more components in the path are missing.
extern int vsd_find_desc_by_id(struct vsd_context* context,
                                       vsd_id_t id,
                                       struct vsd_desc** result);

// Set the boolean value of a signal identified by a descriptor.
//
// Desc is returned by vsd_find_desc_by_id() or vsd_find_desc_by_path().
//
// Return:
//  0 - Value set.
//  EISDIR - Desc points to a branch.
//  EINVAL - Desc is nil
//  EINVAL - Desc is not an uint8_t
//
// FIXME: Validate that val is legal if desc is enumerated.
// FIXME: Validate that val is not greater than maximum allowed value, if specified.
// FIXME: Validate that val is not less than minimum allowed value, if specified.

extern int vsd_set_value_by_desc_boolean(vsd_context_t* context, struct vsd_desc* desc, uint8_t val);

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
extern int vsd_set_value_by_id_boolean(vsd_context_t* context, vsd_id_t id, uint8_t val);

extern int vsd_set_value_by_desc_int8(vsd_context_t* context, struct vsd_desc* desc, int8_t val);
extern int vsd_set_value_by_path_int8(vsd_context_t* context, char* path, int8_t val);
extern int vsd_set_value_by_id_int8(vsd_context_t* context, vsd_id_t id, int8_t val);

extern int vsd_set_value_by_desc_uint8(vsd_context_t* context, struct vsd_desc* desc, uint8_t val);
extern int vsd_set_value_by_path_uint8(vsd_context_t* context, char* path, uint8_t val);
extern int vsd_set_value_by_id_uint8(vsd_context_t* context, vsd_id_t id, uint8_t val);

extern int vsd_set_value_by_desc_int16(vsd_context_t* context, struct vsd_desc* desc, int16_t val);
extern int vsd_set_value_by_path_int16(vsd_context_t* context, char* path, int16_t val);
extern int vsd_set_value_by_id_int16(vsd_context_t* context, vsd_id_t id, int16_t val);

extern int vsd_set_value_by_desc_uint16(vsd_context_t* context, struct vsd_desc* desc, uint16_t val);
extern int vsd_set_value_by_path_uint16(vsd_context_t* context, char* path, uint16_t val);
extern int vsd_set_value_by_id_uint16(vsd_context_t* context, vsd_id_t id, uint16_t val);

extern int vsd_set_value_by_desc_int32(vsd_context_t* context, struct vsd_desc* desc, int32_t val);
extern int vsd_set_value_by_path_int32(vsd_context_t* context, char* path, int32_t val);
extern int vsd_set_value_by_id_int32(vsd_context_t* context, vsd_id_t id, int32_t val);

extern int vsd_set_value_by_desc_uint32(vsd_context_t* context, struct vsd_desc* desc, uint32_t val);
extern int vsd_set_value_by_path_uint32(vsd_context_t* context, char* path, uint32_t val);
extern int vsd_set_value_by_id_uint32(vsd_context_t* context, vsd_id_t id, uint32_t val);

extern int vsd_set_value_by_desc_float(vsd_context_t* context, struct vsd_desc* desc, float val);
extern int vsd_set_value_by_path_float(vsd_context_t* context, char* path, float val);
extern int vsd_set_value_by_id_float(vsd_context_t* context, vsd_id_t id, float val);

extern int vsd_set_value_by_desc_double(vsd_context_t* context, struct vsd_desc* desc, double val);
extern int vsd_set_value_by_path_double(vsd_context_t* context, char* path, double val);
extern int vsd_set_value_by_id_double(vsd_context_t* context, vsd_id_t id, double val);

extern int vsd_set_value_by_desc_string(vsd_context_t* context, struct vsd_desc* desc, char* data, int len);
extern int vsd_set_value_by_path_string(vsd_context_t* context, char* path, char* data, int len);
extern int vsd_set_value_by_id_string(vsd_context_t* context, vsd_id_t id, char* data, int len);

extern int vsd_set_value_by_desc_convert(vsd_context_t* context, struct vsd_desc* desc, char* value);
extern int vsd_set_value_by_path_convert(vsd_context_t* context, char* path, char* value);
extern int vsd_set_value_by_id_convert(vsd_context_t* context, vsd_id_t id, char* value);

// Create a new VSD context
// Create and initialize a new VSD context which can then
// be loaded with a signal specification and subsequent
// pub/sub operations.
extern int vsd_context_create(struct vsd_context** context);


// Set an active context to be used when a signal is received.
// The context defines signal definitions loaded from a VSS CSV file, callbacks, current
// signal values, etc.
// ctx is returned by a prior call to vsd_load_from_file();
//
extern void vsd_set_active_context(vsd_context_t* ctx);

// Return a pointer to the list hosting all children of parent.
// Return 0 if parent is not a branch.
extern vsd_desc_list_t* vsd_get_children(struct vsd_desc* parent);

// Load a signal tree from a CSV file.
// Add a signal to the signals managed by VSD.
// This is a manual variant of vsd_load_from_file() where the information
// of a single signal can be loaded vi an API call instead of from
// a CSV file. Use multiple calls to this function to load an eniter
// specification
// context is created by a prior call to vsd_context_create()
extern int vsd_load_from_file(struct vsd_context* context, char *fname);

// Add a single signal describred by a comma-separated value line.
// Parse the csv_line string, read from a VSS CSV file, and add the signal
// to the context tree
// The CSV line has the following format. All strings are within double quotes ("):
//
// path,id,elem_type,data_type,unit_type,min_max,descr,enum,sensor,actuator
//
// path [str]     - A dot-separated path to the given signal All parent
//                  path components leading up to the signal must have
//                  previously created via a prior call to
//                  vsd_create_desc_from_csv() or
//                  vsd_load_from_file().
//
// id [uint32]    - A unique signal ID for this signal. No two signals can have
//                  the same signal ID
//
// elem_type[str] - Element type. One of attribute, branch, sensor, actuator,
//                  rbranch (ignored), and element.
//
// data_type[str] - Data type. Empty for branch. One of int8, uint8, int16,
//                  uint16, int32, uint32, double, float, boolean,
//                  string, stream (unsupported).
//
// min[data_type] - Minimum value. Empty if N/A and for enumerated values.
//
// max[data_type] - Maximum value. Empty if N/A and for enumerated values.
//
// descr[str]     - Description of signal
//
// enum[str]      - Empty ("") if any value of the correct type is allowed.
//                  slash (/) separated list of values that the signal can have.
//                  No other values are allowed.
//
// sensor[str]    - Reserved. Leave empty("")
//
// actuator[str]  - Reserved. Leave empty("")
//
// Return 0 if ok.
// Return EFAULT if CSV line could not be parsed.
// Return EINVAL if context or csv_line are null.
//
int vsd_create_desc_from_csv(vsd_context_t* context, char *csv_line);

// Convert the given string type to a data type.
// Return vsd_na if conversion was not possible./
extern vsd_data_type_e vsd_string_to_data_type(char* type);

// Convert the given data type to a printable string
extern char* vsd_data_type_to_string(vsd_data_type_e type);

// Convert the given string type to an element type.
// Return vsd_na (from vsd_data_type_e) if conversion
// was not possible.
extern vsd_elem_type_e vsd_string_to_elem_type(char* type);

// Convert the given element type to a printable string
extern char* vsd_elem_type_to_string(vsd_elem_type_e type);

// Install the full path to desc into buf.
// If the path is longer than buf_len characters, only buf_len - 1
// characters will be installed.
// buf will always be null terminated.
extern int vsd_desc_to_path(struct vsd_desc* desc, char* buf, int buf_len);
// Install the full path to desc into a static buf and return it./
// If the path is longer than 1024 bytes, the string
// "[signal path too long]" will be returned.
extern char* vsd_desc_to_path_static(struct vsd_desc* desc);

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
extern int vsd_string_to_data(vsd_data_type_e type,
                              char* str,
                              vsd_data_u* res);

// Publish the signal(s) in desc.
// If desc is a branch, all signals installed under it will be published atomically.
// Unchanged values will be published as well.
extern int vsd_publish(struct vsd_desc* desc);

// Subscribe to signal updates in desc
// If desc is a branch, any updates made to a signal under desc will be reported
// to the callback.
// If an unchanged value is received, it will still trigger a callback.
//
extern int vsd_subscribe(struct vsd_context* ctx,
                                 struct vsd_desc* desc,
                                 vsd_subscriber_cb_t callback);

// Unsubscribe to a descriptor previously subscribed to
extern int vsd_unsubscribe(struct vsd_context* ctx,
                                   struct vsd_desc* desc,
                                   vsd_subscriber_cb_t callback);

// Return the element type of desc.
extern vsd_elem_type_e vsd_elem_type(vsd_desc_t* desc);

// Return the dfata type of desc.
extern vsd_data_type_e vsd_data_type(vsd_desc_t* desc);

// Return the name type of desc.
extern char* vsd_name(vsd_desc_t* desc);

// Return the id  of desc.
extern vsd_id_t vsd_id(vsd_desc_t* desc);

// Return the current value of desc.
extern vsd_data_u vsd_value(vsd_desc_t* desc);

// Return the minimum allowed value to the signal, if specified
extern vsd_data_u vsd_min(vsd_desc_t* desc);

// Return the maximum allowed value to the signal, if specified
extern vsd_data_u vsd_max(vsd_desc_t* desc);
