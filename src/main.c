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

#include "logger.h"
#include "catalog.h"
#include "crossmatch.h"

/**
 * TODO:
 * - hash of Object ipring (in field)
 * - array of pointer to objects charing the same ipring
 * - crossmatch
 */
int
main(int argc, char** argv) {
	int i, j, nfields;
	long nsides = 8192;
	long nzoneindex;
	long *zoneindex = NULL;

    if (argc < 3)
        Logger_log(LOGGER_CRITICAL, "Require two file arguments\n");

	Logger_setLevel(LOGGER_DEBUG);

	nfields = 2;
	Field fields[nfields];

	/* load fields */
	for (i=0, j=1; i<nfields; i++, j++)
		Catalog_open(argv[1], &fields[i], nsides);


	/* create a kind of zone database ... */
	ObjectZone *zone = Catalog_initzone(nsides);
	nzoneindex = Catalog_fillzone(fields, nfields, zone, nsides, &zoneindex);

	Logger_log(LOGGER_DEBUG, "Got %li zones for all fields\n", nzoneindex);
    Logger_log(LOGGER_DEBUG, "ttttttttttttttttttt %p\n", zoneindex);

	/* ... that will speed up cross matching */
	//Crossmatch_cross(fields, nfields, zone);
	Crossmatch_crosszone(zone, zoneindex, nzoneindex);

	/* cleanup */
	for (i=0; i<nfields; i++)
		Catalog_freefield(&fields[i]);
	Catalog_freezone(zone, nsides);

	return (EXIT_SUCCESS);

}

