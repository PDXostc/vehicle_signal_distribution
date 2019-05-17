
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
#include "vehicle_signal_distribution.h"
#include "dstc.h"

void usage(char* prog)
{
        fprintf(stderr, "Usage: %s-p <signal->path> -s:<signal-path:value> ...\n", prog);
        fprintf(stderr, "  -p <signal->path>        The signal tree to publish\n");
        fprintf(stderr, "  <signal-path:value> ...  Signal-value pair to set before publish\n\n");
        fprintf(stderr, "Example: %s -p Vehicle.Drivetrain.InternalCombustionEngine \\\n",
                prog);
        fprintf(stderr, "         %*c -s Vehicle.Drivetrain.InternalCombustionEngine.Engine.Power:230 \\\n",
                (int) strlen(prog), ' ');
        fprintf(stderr, "         %*c -s Vehicle.Drivetrain.InternalCombustionEngine.FuelType:gasoline\n",
                (int) strlen(prog), ' ');

        exit(255);

}

int main(int argc, char* argv[])
{
    vss_signal_t* sig;
    int res;
    char opt = 0;
    char sig_path[1024];
    char *val;

    if (argc == 1)
        usage(argv[0]);


    optind = 1;
    while ((opt = getopt(argc, argv, "s:p:")) != -1) {
        switch (opt) {

        case 's':
            strcpy(sig_path, optarg);
            val = strchr(sig_path, ':');
            if (!val) {
                fprintf(stderr, "Missing colon. Please signal values to set as -s <path>:<value>\n");
                exit(255);
            }
            *val++ = 0;

            res = vsd_set_value_by_path_convert(0, sig_path, val);
            if (res) {
                fprintf(stderr, "Could not set %s to %s: %s\n", sig_path, val, strerror(res));
                exit(255);
            }
            break;

        case 'p':
            fprintf(stderr, "Publishing %s\n", optarg);
            res = vss_get_signal_by_path(optarg, &sig);
            if (res) {
                fprintf(stderr, "Could not use publish path %s: %s\n", optarg, strerror(res));
                exit(255);
            }
            break;

        default: /* '?' */
            break;
        }
    }

    // Did we specify the signal tree to publish.
    if (!sig) {
        fprintf(stderr, "Please specify the signal subtree to publish using -p <signal->path>\n");
        exit(255);
    }

    // Ensure that the pub-sub relationships have been setup
    dstc_process_events(400000);

    // Send publish command to update
    res = vsd_publish(sig);

    if (res) {
        printf("Cannot publish signal %s %s\n", argv[2], strerror(res));
        exit(255);
    }

    // Process signals for another 100 msec to ensure that the call gets out.
    dstc_process_events(100000);
}
