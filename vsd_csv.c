// Copyright (C) 2018, Jaguar Land Rover
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (mfeuer1@jaguarlandrover.com)
//
// Read CSV-based signal files and build up an signal structure
//
#include "vsd.h"
#include "vsd_internal.h"
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "rmc_common.h"
#include "rmc_log.h"


static int get_next_token(char* csv_line, int* index, char* result, int max_result_len)
{
    char quoted=0;
    int r_len = 0;
    while(csv_line[*index] &&
          (csv_line[*index] != ',' || quoted) &&
          r_len < max_result_len) {

        if (csv_line[*index] != '\"') {
            r_len++;
            *result++ = csv_line[*index];
        } else
            quoted = quoted?0:1;

        (*index)++;
    }

    // Have we reached end of the line?
    if (!csv_line[*index])
        return 0;


    // Have we filled result?
    if (r_len > 0 && r_len == max_result_len) {
        *(result - 1) = 0; // Null terminate last legal character
        return ENOSPC;
    }

    // Null-terminate the token
    *result = 0;

    // Sanity check
    if (csv_line[*index] != ',')
        return EINVAL;

    (*index)++;
    return EAGAIN; // More to have.
}


int vsd_string_to_data(vsd_data_type_e type, char* str, vsd_data_u* res)
{
    switch(type) {
    case vsd_int8: { *res = (vsd_data_u) { .i8 = (int8_t) strtol(str, 0, 10) }; return 0; }
    case vsd_uint8: { *res = (vsd_data_u) { .u8 = (uint8_t) strtoul(str, 0, 10) }; return 0; }
    case vsd_int16: { *res = (vsd_data_u) { .i16 = (int16_t) strtol(str, 0, 10) }; return 0; }
    case vsd_uint16: { *res = (vsd_data_u) { .u16 = (uint16_t) strtoul(str, 0, 10) }; return 0; }
    case vsd_int32: { *res = (vsd_data_u) { .i32 = (int32_t) strtol(str, 0, 10) }; return 0; }
    case vsd_uint32: { *res = (vsd_data_u) { .u32 = (uint32_t) strtoul(str, 0, 10) }; return 0; }

    case vsd_double: { *res = (vsd_data_u) { .d = (double) strtod(str, 0) }; return 0; }
    case vsd_float: { *res = (vsd_data_u) { .f = (float) strtof(str, 0) }; return 0; }
    case vsd_boolean: { *res = (vsd_data_u) { .b = (*str == '1' || *str == 't' || *str == 'T')?1:0 }; return 0; }
    case vsd_string: { *res = (vsd_data_u) { .s.data = str, .s.len = strlen(str)+1 }; return 0; }
    default:
        RMC_LOG_WARNING("Illegal type: %d / %s\n", type, str);
        *res = vsd_data_u_nil;
        return EINVAL;
    }
}


static vsd_desc_t* create_branch(vsd_context_t* context,
                                         vsd_desc_branch_t* parent,
                                         char* name,
                                         vsd_id_t id,
                                         char* desc)
{
    vsd_desc_branch_t* res = 0;

    vsd_desc_create_branch(context, &res, name, id, desc, parent);
    return &res->base;
}


static vsd_desc_t* create_leaf(vsd_context_t* context,
                                      vsd_elem_type_e elem_type,
                                      vsd_data_type_e data_type,
                                      vsd_desc_branch_t* parent,
                                      char* name,
                                      vsd_id_t id,
                                      char* desc,
                                      vsd_data_u min,
                                      vsd_data_u max)
{
    vsd_desc_leaf_t* res = 0;

    vsd_desc_create_leaf(context,
                                &res,
                                elem_type,
                                data_type,
                                id,
                                name,
                                desc,
                                parent,
                                min,
                                max,
                                vsd_data_u_nil);
    return &res->base;

}

static vsd_desc_t* create_enumerator(vsd_context_t* context,
                                            vsd_elem_type_e elem_type,
                                            vsd_data_type_e data_type,
                                            vsd_desc_branch_t* parent,
                                            char* name,
                                            vsd_id_t id,
                                            char* desc,
                                            char* enumerator)
{
    vsd_desc_enum_t* res = 0;
    char* str = enumerator;
    char* token = 0;
    char* saved = 0;


    int enum_count = 0;

    // Count the number of elements.
    while((token = strtok_r(str, " /", &saved))) {
        str = 0; // See man strtok_r(3)
        enum_count++;
    }

    vsd_data_u enum_data[enum_count];

    // Fill out the element strings
    str = enumerator;
    saved = 0;
    enum_count = 0;

    // Count the number of elements.
    while((token = strtok_r(str, " /", &saved))) {
        str = 0; // See man strtok_r(3)
        vsd_string_to_data(data_type, token, &enum_data[enum_count]);

        // Make a copy of a string-based enum since we will lose the original
        // once this functionr eturn.
        if (data_type == vsd_string)
            enum_data[enum_count].s.data = strdup(enum_data[enum_count].s.data);

        enum_count++;
    }

    vsd_desc_create_enum(context,
                         &res,
                         elem_type,
                         data_type,
                         id,
                         name,
                         desc,
                         parent,
                         enum_data,
                         enum_count,
                         vsd_data_u_nil);

    return &res->leaf.base;
}


vsd_desc_t* vsd_desc_create_from_csv(vsd_context_t* context,
                                                   char *csv_line)
{
    char path[1024];
    char id[16];
    char elem_type[48];
    char data_type[40];
    char unit_type[40];
    char min[40];
    char max[40];
    char desc_str[1024];
    char enumerator[1024];
    char sensor[1024];
    char actuator[1024];
    int res = 0;
    int index = 0;
    char *final_path_component = 0;
    vsd_desc_branch_t* parent;
    vsd_desc_t* parent_desc;
    vsd_elem_type_e elem_type_enum;
    vsd_data_type_e data_type_enum;

    // Check arguments
    if (!context || !csv_line)
        return 0;

    RMC_LOG_DEBUG("Creating signal from: [%s]", csv_line);

    if (get_next_token(csv_line, &index, path, sizeof(path)) != EAGAIN ||
        get_next_token(csv_line, &index, id, sizeof(id)) != EAGAIN ||
        get_next_token(csv_line, &index, elem_type, sizeof(elem_type)) != EAGAIN ||
        get_next_token(csv_line, &index, data_type, sizeof(data_type)) != EAGAIN ||
        get_next_token(csv_line, &index, unit_type, sizeof(unit_type)) != EAGAIN ||
        get_next_token(csv_line, &index, min, sizeof(min)) != EAGAIN ||
        get_next_token(csv_line, &index, max, sizeof(max)) != EAGAIN ||
        get_next_token(csv_line, &index, desc_str, sizeof(desc_str)) != EAGAIN ||
        get_next_token(csv_line, &index, enumerator, sizeof(enumerator)) != EAGAIN ||
        get_next_token(csv_line, &index, sensor, sizeof(sensor)) != EAGAIN ||
        get_next_token(csv_line, &index, actuator, sizeof(actuator)) != 0) {
        RMC_LOG_WARNING("Malformed signal descriptor CSV line ignored: [%s]", csv_line);
        return 0;
    }

    // Break up path in path and final component.
    final_path_component = strrchr(path, '.');

    // Check if we have a multi-component path
    if (final_path_component) {
        *final_path_component++ = 0;

        // Locate the correct parent branch
        res = vsd_find_desc_by_path(context,
                                            &context->root->base,
                                            path,
                                            &parent_desc);

        if (res != 0) {
            RMC_LOG_FATAL("Could not find existing path to signal [%s]: %s",
                          path, strerror(res));
            exit(255);
        }

        if (parent_desc->elem_type != vsd_branch) {
            RMC_LOG_FATAL("Parent element is not branch [%s]: %s",
                          path, vsd_elem_type_to_string(parent_desc->elem_type));
            exit(255);
        }
        parent = (vsd_desc_branch_t*) parent_desc;
    } else {
        parent = 0; // Install under context root.
        final_path_component = path;
    }

    elem_type_enum = vsd_string_to_elem_type(elem_type);
    data_type_enum = vsd_string_to_data_type(data_type);

    // Is this an enumerated thing?
    if (strlen(enumerator))
        return create_enumerator(context,
                                 elem_type_enum,
                                 data_type_enum,
                                 parent,
                                 final_path_component,
                                 strtoul(id, 0, 0),
                                 desc_str,
                                 enumerator);

    if (elem_type_enum == vsd_branch) {
        RMC_LOG_DEBUG("BRANCH Creating %s", final_path_component);
        return create_branch(context,
                             parent,
                             final_path_component,
                             strtoul(id, 0, 0),
                             desc_str);
    }

    switch(data_type_enum) {
    case vsd_int8:
    case vsd_uint8:
    case vsd_int16:
    case vsd_uint16:
    case vsd_int32:
    case vsd_uint32:
    case vsd_double:
    case vsd_float:
    case vsd_boolean: {
        vsd_data_u min_u;
        vsd_data_u max_u;

        vsd_string_to_data(data_type_enum, min, &min_u);
        vsd_string_to_data(data_type_enum, max, &max_u);
        RMC_LOG_DEBUG("LEAF Creating %s as %s", final_path_component, vsd_data_type_to_string(data_type_enum));
        return create_leaf(context,
                           elem_type_enum,
                           data_type_enum,
                           parent,
                           final_path_component,
                           strtoul(id, 0, 0),
                           desc_str,
                           min_u,
                           max_u);
    }
    case vsd_string: {
        RMC_LOG_DEBUG("STRING Creating %s as %s", final_path_component, vsd_data_type_to_string(data_type_enum));
        return create_leaf(context,
                           elem_type_enum,
                           data_type_enum,
                           parent,
                           final_path_component,
                           strtoul(id, 0, 0),
                           desc_str,
                           vsd_data_u_nil,
                           vsd_data_u_nil);
    }
    default: {
        RMC_LOG_WARNING("Ignoring signal type [%s]", data_type);
        return 0;
    }
    }
}

int vsd_load_from_file(vsd_context_t** context, char *fname)
{
    FILE* in = fopen(fname, "r");
    char buf[4096];
    int line = 1;

    *context = (vsd_context_t*) malloc(sizeof(vsd_context_t));
    if (!*context) {
        RMC_LOG_ERROR("Could not allocate %lu bytes: %s",
                      sizeof(vsd_context_t), strerror(errno));
        exit(255);
    }
    vsd_context_init(*context);
    vsd_set_active_context(*context);
    dstc_setup();

    if (!fname) {
        RMC_LOG_WARNING("Could not open %s: %s", fname, strerror(errno));
        return ENOENT;
    }

    while(fgets(buf, sizeof(buf), in)) {
        if (buf[strlen(buf)-1] == '\n')
            buf[strlen(buf)-1] = 0;
        RMC_LOG_DEBUG("Processing line %d", line);

        vsd_desc_create_from_csv(*context, buf);
        ++line;
    }
    return 0;
}
