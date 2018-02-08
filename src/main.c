/*
 * Copyright (C) 2017 University of Bordeaux. All right reserved.
 * Written by Emmanuel Bertin
 * Written by Sebastien Serre
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include "mem.h"
#include "logger.h"
#include "catalog.h"
#include "crossmatch.h"

#include "chealpix.h"
#include "scamp.h"

/**
 * TODO:
 * 1 - get nsides depending of the max error from all files with the neighbors
 * algo. (see dist2holes_nest fortran)
 * 2 - cross/match with as little as possible samples (see query_ring?).
 */
int main(int argc, char** argv) {

    Logger_setLevel(LOGGER_NORMAL);

    /* default values */
    int nsides_power = 16, c;
    double radius_arcsec = 2.0; /* in arcsec */

    while ((c=getopt(argc,argv,"n:r:b")) != -1) {
        switch(c) {
        case 'n':
            nsides_power = atoi(optarg);
            break;
        case 'r':
            radius_arcsec = atof(optarg);
            break;
        default:
            abort();
        }
    }

    int nfields   = argc - optind;
    char **cat_files = &argv[optind];

    Field *fields = ALLOC(sizeof(Field) * nfields);

    int i;
    for (i=0; i<nfields; i++)
        Catalog_open(cat_files[i], &fields[i]);

    time_t real_start, real_end;
    clock_t start, end;
    double cpu_time_used;
    double real_time_used;
    int64_t nsides = pow(2, nsides_power);
	printf("match radius max is %0.30lf\n", (180.0f / (4 * nsides - 1)) * 3600  );
    start = clock();
    real_start = time(NULL);
    Crossmatch_crossFields(fields, nfields, nsides, radius_arcsec);
    end = clock();
    real_end = time(NULL);

    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Crossmatch done in %lf cpu_time seconds and %lf real seconds\n", cpu_time_used, real_time_used);

    for (i=0; i<nfields; i++)
        Catalog_freeField(&fields[i]);

    return (EXIT_SUCCESS);

}

