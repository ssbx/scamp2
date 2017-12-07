/*
 * global.h
 *
 *  Created on: 29 nov. 2017
 *      Author: serre
 */

#ifndef SRC_SCAMP_H_
#define SRC_SCAMP_H_

#define SC_PI 3.141592653589793238462643383279502884197
#define SC_TWOPI 6.283185307179586476925286766559005768394
#define SC_HALFPI 1.570796326794896619231321691639751442099
#define SC_INV_HALFPI 0.6366197723675813430755350534900574
#define SC_PI_DIV_180 0.01745329251994329576923690768488612713442

#include <wcslib/wcshdr.h>

typedef struct Sample Sample;
typedef struct Set Set;
typedef struct Field Field;
typedef struct MatchBundle MatchBundle;

/**
 * Sample structure represent a set entry. ra and dec are both represented
 * in degree (for wcslib), radiant and vectors (for healpix).
 */
struct Sample {

    /* same as "number" in sextractor catalog */
    long id;

    /* Default unit is radiant, used by healpix */
    double ra;     /* right ascension (rad) (x) world coordinates */
    double dec;    /* declination     (rad) (y) world coordinates */

    /* Degrees used by WCS */
    double raDeg;   /* ra in degree */
    double decDeg;  /* dec in degree */

    /* Representation as vector used to determinate the angle distance
     * with another vector in the cross-match algorithm (see angdist) */
    double vector[3];

    /* position on healpix nested scheme */
    long pix_nest;

    /* Sample belonging to this set */
    Set *set;

    /* Best matching sample from another field */
    Sample *bestMatch;

    /* Best distance is the distance to bestMatch in radiant. It is initialized
     * in the Crossmatch_crossSeel function to the value of max radius and used
     * in the cross matching algorithm. May contain a value (initial radius)
     * without a bestMatch sample
     */
    double bestMatchDistance;

    /* Object belong to this match bundle. */
    MatchBundle *matchBundle;

};

/**
 * Set's sample share a common image source (a CCD).
 */
struct Set {

    Sample *samples;
    int     nsamples;

    /*
     * wcsprm structures used by wcslib to convert from pixel to world
     * coordinates and vice versa. (see wcsp2s and wcss2p)
     */
    struct wcsprm *wcs;
    int nwcs;

    Field *field;

};


/**
 * A field represent a file containing Set(s).
 */
struct Field {

    Set *sets;
    int  nsets;

};

/**
 * A match bundle contains every samples from any fields that match each others.
 * This include friend-of-friends objects.
 */
struct MatchBundle {
    Sample **samples;
    int     nsamples;
};

#endif /* SRC_SCAMP_H_ */
