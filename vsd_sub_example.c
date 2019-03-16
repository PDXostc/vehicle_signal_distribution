// Copyright (C) 2018, Jaguar Land Rover
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//

#include <stdio.h>
#include <stdlib.h>
#include "dstc.h"
#include "vehicle_signal_distribution.h"

void dump_desc(vsd_desc_t* elem)
{
    if (vsd_elem_type(elem) == vsd_branch) {
        printf("FATAL: Tried to print branch: %u:%s", vsd_id(elem), vsd_name(elem));
        exit(255);
    }

    printf("%s - %u:%s:%s -> ",
           vsd_name(elem),
           vsd_id(elem),
           vsd_elem_type_to_string(vsd_elem_type(elem)),
           vsd_data_type_to_string(vsd_data_type(elem)));

    switch(vsd_data_type(elem)) {
    case vsd_int8:
        printf("%d\n", vsd_value(elem).i8 & 0xFF);
        break;
    case vsd_int16:
        printf("%d\n", vsd_value(elem).i16 & 0xFFFF);
        break;
    case vsd_int32:
        printf("%d\n", vsd_value(elem).i32);
        break;
    case vsd_uint8:
        printf("%u\n", vsd_value(elem).u8 & 0xFF);
        break;
    case vsd_uint16:
        printf("%u\n", vsd_value(elem).u16 & 0xFFFF);
        break;
    case vsd_uint32:
        printf("%u\n", vsd_value(elem).u32);
        break;
    case vsd_double:
        printf("%f\n", vsd_value(elem).d);
        break;
    case vsd_float:
        printf("%f\n", vsd_value(elem).f);
        break;
    case vsd_boolean:
        printf("%s\n", vsd_value(elem).b?"true":"false");
        break;

    case vsd_string:
        if (vsd_value(elem).s.len)
            puts(vsd_value(elem).s.data);
        else
            puts("[nil]");
        break;
    case vsd_na:
        printf("[n/a]\n");
        break;
    default:
        printf("[unsupported]\n");
    }
    return;
}

void signal_sub(vsd_desc_list_t* list)
{
    vsd_desc_list_for_each(list,
                           lambda(uint8_t,
                                  (vsd_desc_node_t* node, void* _ud) {
                                      dump_desc(node->data);
                                      return 1;
                                  }), 0);

}


int main(int argc, char* argv[])
{
    vsd_desc_t* desc = 0;
    vsd_context_t* ctx = 0;
    int res = 0;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <signal_desc.csv> <signal-path>\n", argv[0]);
        fprintf(stderr, "Example: %s vss_rel_2.0.0-alpha+005.csv Vehicle.Drivetrain.InternalCombustionEngine\n", argv[0]);
        fprintf(stderr, "Signal descriptor CSV file is installed in the share directory under the\n");
        fprintf(stderr, "top-level installation directory.\n");
        exit(255);
    }


    // Load descriptor file
    res = vsd_load_from_file(&ctx, argv[1]);

    if (res) {
        printf("Cannot load file %s: %s\n", argv[1], strerror(res));
        exit(255);
    }


    // Find the signal descriptor we want to subscribe to
    // Descriptor can be anywhere in the signal tree. Branch or signal
    res = vsd_find_desc_by_path(ctx, 0, argv[2], &desc);
    if (res) {
        printf("Cannot find signal %s: %s\n", argv[2], strerror(res));
        exit(255);
    }

    // Subscribe to the signal
    res = vsd_subscribe(ctx, desc, signal_sub);
    if (res) {
        printf("Cannot subscribe to signal %s: %s\n", argv[2], strerror(res));
        exit(255);
    }

    // Process incoming events forever.
    dstc_process_events(-1);
}
