/*
 * test_crossmatch_limit.c
 *
 *  Created on: 8 déc. 2017
 *      Author: serre
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "../src/scamp.h"
#include "../src/mem.h"
#include "../src/catalog.h"
#include "../src/crossmatch.h"

extern void test_Catalog_open_ascii(char*, Field*);

/*
 * There should be only one match between t1 and t3
 */
static char t1[] = "tests/data/asciicat/t1_cat.txt";
static char t3[] = "tests/data/asciicat/t3_cat.txt";

int main(int argc, char **argv) {

    Field fields[2];
    test_Catalog_open_ascii(t1, &fields[0]);
    test_Catalog_open_ascii(t3, &fields[1]);

    long nsides = pow(2, 10);
    double radius_arcsec = 2.0;
    long matches;
    matches = Crossmatch_crossFields(fields, 2, nsides, radius_arcsec,
            ALGO_NEIGHBORS, STORE_SCHEME_AVLTREE);

    assert(matches == 2);

    Catalog_freeField(&fields[0]);
    Catalog_freeField(&fields[1]);
    return 0;
}
