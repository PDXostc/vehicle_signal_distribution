
// Copyright (C) 2018, Jaguar Land Rover
// This program is licensed under the terms and conditions of the
// Mozilla Public License, version 2.0.  The full text of the
// Mozilla Public License is at https://www.mozilla.org/MPL/2.0/
//
// Author: Magnus Feuer (mfeuer1@jaguarlandrover.com)
//
// Event publishing
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vsd.h"
#include "dstc.h"

int main(int argc, char* argv[])
{
    vsd_desc_t* desc;
    vsd_data_u val;
    vsd_context_t* ctx = 0;
    int res;
    if (argc != 4) {
        fprintf(stderr, "Usage: %s -s <signal_desc.csv> -p <signal->path> <signal-path:value> ...\n", argv[0]);
        fprintf(stderr, "  -s <signal_desc.csv>     Signal specification from github.com/GENIVI/vehicle_signal_manager\n");
        fprintf(stderr, "  -p <signal->path>        The signal tree to publish\n");
        fprintf(stderr, "  <signal-path:value> ...  Signal-value pair to set before publish\n\n");
        fprintf(stderr, "Example: %s -s vss_rel_2.0.0.csv \\\n", argv[0]);
        fprintf(stderr, "         %*c -p Vehicle.Drivetrain.InternalCombustionEngine \\\n", (int) strlen(argv[0]), ' ');
        fprintf(stderr, "         %*c    Vehicle.Drivetrain.InternalCombustionEngine.Engine.Power:230 \\\n",(int) strlen(argv[0]), ' ');
        fprintf(stderr, "         %*c    Vehicle.Drivetrain.InternalCombustionEngine.FuelType:gasoline \\\n", (int) strlen(argv[0]), ' ');

        exit(255);
   }

    vsd_load_from_file(&ctx, argv[1]);
    res = vsd_find_desc_by_path(ctx, 0, argv[2], &desc);

    if (res) {
        printf("Cannot find signal %s: %s\n", argv[2], strerror(res));
        exit(255);
    }

    if (vsd_elem_type(desc) == vsd_branch) {
        printf("Cannot signal %s is a branch: %s\n", argv[2], strerror(res));
        exit(255);
    }

    res = vsd_string_to_data(vsd_data_type(desc), argv[3], &val);


    if (res) {
        printf("Cannot set convert %s to value of type %s: %s\n",
               argv[3], vsd_data_type_to_string(vsd_data_type(desc)), strerror(res));
        exit(255);
    }

    vsd_set_value(desc, val);

    // Ensure that the pub-sub relationships have been setup
    dstc_process_events(500000);

    res = vsd_publish(desc);

    if (res) {
        printf("Cannot publish signal %s %s\n", argv[2], strerror(res));
        exit(255);
    }

    // Process signals for another 100 msec to ensure that the call gets out.
    dstc_process_events(100000);
}
