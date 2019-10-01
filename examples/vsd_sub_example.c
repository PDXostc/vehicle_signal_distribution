// Copyright (C) 2018, Jaguar Land Rover
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//

#include <stdio.h>
#include <stdlib.h>
#include "dstc.h"
#include "vehicle_signal_distribution.h"
#include "vss.h"
#include "vss_macro.h"

int exit_flag = 0;

void dump_desc(vss_signal_t* elem)
{
    vsd_data_u val;

    if (elem->element_type == VSS_BRANCH) {
        printf("FATAL: Tried to print branch: %s", vsd_signal_to_path_static(elem));
        exit(255);
    }

    vsd_get_value(elem, &val);

    printf("%s - %s:%s -> ",
           elem->name,
           vss_element_type_string(elem->element_type),
           vss_data_type_string(elem->data_type));


    // data_type is the same as sig->data_type
    switch(elem->data_type) {
    case VSS_INT8:
        printf("%d\n", val.i8 & 0xFF);
        break;
    case VSS_INT16:
        printf("%d\n", val.i16 & 0xFFFF);
        break;
    case VSS_INT32:
        printf("%d\n", val.i32);
        break;
    case VSS_UINT8:
        printf("%u\n", val.u8 & 0xFF);
        break;
    case VSS_UINT16:
        printf("%u\n", val.u16 & 0xFFFF);
        break;
    case VSS_UINT32:
        printf("%u\n", val.u32);
        break;
    case VSS_DOUBLE:
        printf("%f\n", val.d);
        break;
    case VSS_FLOAT:
        printf("%f\n", val.f);
        break;
    case VSS_BOOLEAN:
        printf("%s\n", val.b?"true":"false");
        break;

    case VSS_STRING:
        if (val.s.len)
            puts(val.s.data);
        else
            puts("[nil]");
        break;
    case VSS_NA:
        printf("[n/a]\n");
        break;
    default:
        printf("[unsupported]\n");
    }
    return;
}

static uint8_t _dump_description(vsd_signal_node_t* node, void* _ud) {
    dump_desc(node->data);
    return 1;
}

void signal_sub(vsd_context_t* ctx,vsd_signal_list_t* list)
{
    puts("Got signal");
    vsd_signal_list_for_each(list, _dump_description, 0);

    puts("----\n");
    exit_flag = 1;

}


int main(int argc, char* argv[])
{
    vss_signal_t* sig = 0;
    int res = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <signal-path>\n", argv[0]);
        fprintf(stderr, "Example: %s Vehicle.Drivetrain.InternalCombustionEngine\n", argv[0]);
        exit(255);
    }


    // Find the signal sigriptor we want to subscribe to
    // Sigriptor can be anywhere in the signal tree. Branch or signal
    res = vss_get_signal_by_path(argv[1], &sig);
    if (res) {
        printf("Cannot find signal %s: %s\n", argv[1], strerror(res));
        exit(255);
    }

    // Subscribe to the signal
    res = vsd_subscribe(0, sig, signal_sub);
    if (res) {
        printf("Cannot subscribe to signal %s: %s\n", argv[1], strerror(res));
        exit(255);
    }

    // Process incoming events forever.
     while(exit_flag == 0)
        dstc_process_events(-1);
}
