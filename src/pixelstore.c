/*
 * Healpix pixels storage mechanism.
 *
 * This file is divided in four parts:
 * - 1 AVL tree implementations,
 * - 2 static functions
 * - 3 public interface
 *
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

#include "pixelstore.h"
#include "chealpix.h"
#include "mem.h"
#include "assert.h"
#include "string.h"
#include "logger.h"

/*****************************************************************************
 * 1 AVL Tree implementation
 *
 * From the SQLite source code ext/misc/amatch.c and ext/misc/closure.c with
 * the following notice:
 *
 * The author disclaims copyright to this source code.  In place of
 * a legal notice, here is a blessing:
 *
 *    May you do good and not evil.
 *    May you find forgiveness for yourself and forgive others.
 *    May you share freely, never taking more than you give.
 */
typedef struct pixel_avl pixel_avl;
struct pixel_avl {
    HealPixel pixel; /* The samples being stored. Key is at pixel.id */
    pixel_avl *pBefore; /* Other elements less than pixel.id */
    pixel_avl *pAfter; /* Other elements greater than pixel.id */
    pixel_avl *pUp; /* Parent element */
    short int height; /* Height of this node.  Leaf==1 */
    short int imbalance; /* Height difference between pBefore and pAfter */
};

static int pixel_cmp(long a, long b) {return a < b ? -1 : (a > b ? 1 : 0);}

/* Recompute the amatch_avl.height and amatch_avl.imbalance fields for p.
 ** Assume that the children of p have correct heights.
 */
static void amatchAvlRecomputeHeight(pixel_avl *p) {
    short int hBefore = p->pBefore ? p->pBefore->height : 0;
    short int hAfter = p->pAfter ? p->pAfter->height : 0;
    p->imbalance = hBefore - hAfter; /* -: pAfter higher.  +: pBefore higher */
    p->height = (hBefore > hAfter ? hBefore : hAfter) + 1;
}

/*
 **     P                B
 **    / \              / \
 **   B   Z    ==>     X   P
 **  / \                  / \
 ** X   Y                Y   Z
 **
 */
static pixel_avl *amatchAvlRotateBefore(pixel_avl *pP) {
    pixel_avl *pB = pP->pBefore;
    pixel_avl *pY = pB->pAfter;
    pB->pUp = pP->pUp;
    pB->pAfter = pP;
    pP->pUp = pB;
    pP->pBefore = pY;
    if (pY)
        pY->pUp = pP;
    amatchAvlRecomputeHeight(pP);
    amatchAvlRecomputeHeight(pB);
    return pB;
}

/*
 **     P                A
 **    / \              / \
 **   X   A    ==>     P   Z
 **      / \          / \
 **     Y   Z        X   Y
 **
 */
static pixel_avl *amatchAvlRotateAfter(pixel_avl *pP) {
    pixel_avl *pA = pP->pAfter;
    pixel_avl *pY = pA->pBefore;
    pA->pUp = pP->pUp;
    pA->pBefore = pP;
    pP->pUp = pA;
    pP->pAfter = pY;
    if (pY)
        pY->pUp = pP;
    amatchAvlRecomputeHeight(pP);
    amatchAvlRecomputeHeight(pA);
    return pA;
}

/*
 ** Return a pointer to the pBefore or pAfter pointer in the parent
 ** of p that points to p.  Or if p is the root node, return pp.
 */
static pixel_avl **pixelAvrFromPtr(pixel_avl *p, pixel_avl **pp) {
    pixel_avl *pUp = p->pUp;
    if (pUp == 0)
        return pp;
    if (pUp->pAfter == p)
        return &pUp->pAfter;
    return &pUp->pBefore;
}

/*
 ** Rebalance all nodes starting with p and working up to the root.
 ** Return the new root.
 */
static pixel_avl *pixelAvlBalance(pixel_avl *p) {
    pixel_avl *pTop = p;
    pixel_avl **pp;
    while (p) {
        amatchAvlRecomputeHeight(p);
        if (p->imbalance >= 2) {
            pixel_avl *pB = p->pBefore;
            if (pB->imbalance < 0)
                p->pBefore = amatchAvlRotateAfter(pB);
            pp = pixelAvrFromPtr(p, &p);
            p = *pp = amatchAvlRotateBefore(p);
        } else if (p->imbalance <= (-2)) {
            pixel_avl *pA = p->pAfter;
            if (pA->imbalance > 0)
                p->pAfter = amatchAvlRotateBefore(pA);
            pp = pixelAvrFromPtr(p, &p);
            p = *pp = amatchAvlRotateAfter(p);
        }
        pTop = p;
        p = p->pUp;
    }
    return pTop;
}

/* Search the tree rooted at p for an entry with zId.  Return a pointer
 ** to the entry or return NULL.
 */
pixel_avl *pixelAvlSearch(pixel_avl *p, const long zId) {
    int c;
    while (p && (c = pixel_cmp(zId, p->pixel.id)) != 0) {
        p = (c < 0) ? p->pBefore : p->pAfter;
    }
    return p;
}

/* Insert a new node pNew.  Return NULL on success.  If the key is not
 ** unique, then do not perform the insert but instead leave pNew unchanged
 ** and return a pointer to an existing node with the same key.
 */
pixel_avl *pixelAvlInsert(pixel_avl **ppHead, pixel_avl *pNew) {
    int c;
    pixel_avl *p = *ppHead;
    if (p == 0) {
        p = pNew;
        pNew->pUp = 0;
    } else {
        while (p) {
            c = pixel_cmp(pNew->pixel.id, p->pixel.id);
            if (c < 0) {
                if (p->pBefore) {
                    p = p->pBefore;
                } else {
                    p->pBefore = pNew;
                    pNew->pUp = p;
                    break;
                }
            } else if (c > 0) {
                if (p->pAfter) {
                    p = p->pAfter;
                } else {
                    p->pAfter = pNew;
                    pNew->pUp = p;
                    break;
                }
            } else {
                return p;
            }
        }
    }
    pNew->pBefore = 0;
    pNew->pAfter = 0;
    pNew->height = 1;
    pNew->imbalance = 0;
    *ppHead = pixelAvlBalance(p);
    return 0;
}

void pixelAvlFree(pixel_avl *pix) {
    if (!pix)
        return;

    pixelAvlFree(pix->pAfter);
    pixelAvlFree(pix->pBefore);

    FREE(pix);
}

#if 0 /* NOT USED */
/* Remove node pOld from the tree.  pOld must be an element of the tree or
** the AVL tree will become corrupt.
*/
static void amatchAvlRemove(amatch_avl **ppHead, amatch_avl *pOld){
    amatch_avl **ppParent;
    amatch_avl *pBalance = 0;
    /* assert( amatchAvlSearch(*ppHead, pOld->zKey)==pOld ); */
    ppParent = amatchAvlFromPtr(pOld, ppHead);
    if( pOld->pBefore==0 && pOld->pAfter==0 ){
        *ppParent = 0;
        pBalance = pOld->pUp;
    }else if( pOld->pBefore && pOld->pAfter ){
        amatch_avl *pX, *pY;
        pX = amatchAvlFirst(pOld->pAfter);
        *amatchAvlFromPtr(pX, 0) = pX->pAfter;
        if( pX->pAfter ) pX->pAfter->pUp = pX->pUp;
        pBalance = pX->pUp;
        pX->pAfter = pOld->pAfter;
        if( pX->pAfter ){
            pX->pAfter->pUp = pX;
        }else{
            assert( pBalance==pOld );
            pBalance = pX;
        }
        pX->pBefore = pY = pOld->pBefore;
        if( pY ) pY->pUp = pX;
        pX->pUp = pOld->pUp;
        *ppParent = pX;
    }else if( pOld->pBefore==0 ){
        *ppParent = pBalance = pOld->pAfter;
        pBalance->pUp = pOld->pUp;
    }else if( pOld->pAfter==0 ){
        *ppParent = pBalance = pOld->pBefore;
        pBalance->pUp = pOld->pUp;
    }
    *ppHead = amatchAvlBalance(pBalance);
    pOld->pUp = 0;
    pOld->pBefore = 0;
    pOld->pAfter = 0;
    /* assert( amatchAvlIntegrity(*ppHead) ); */
    /* assert( amatchAvlIntegrity2(*ppHead) ); */
}
#endif
/**
 * AVL related functions end
 ******************************************************************************/



/******************************************************************************
 * 2 PRIVATE FUNCTIONS
 */
#define SPL_BASE_SIZE 50
static void
insert_sample_into_bigarray_store(Sample *spl, PixelStore *store,
                        long index, long nsides) {
    HealPixel **pixels = store->pixels;
    HealPixel *pix;

    if (pixels[index] == NULL) {
        pixels[index] = ALLOC(sizeof(HealPixel));
        pixels[index]->samples = ALLOC(sizeof(Sample*) * SPL_BASE_SIZE);
        pixels[index]->size = SPL_BASE_SIZE;
        pixels[index]->nsamples = 0;
        pixels[index]->id = spl->pix_nest;
        neighbours_nest(nsides, index, pixels[index]->neighbors);
        if (store->npixels == store->pixelids_size) {
            store->pixelids = REALLOC(store->pixelids, sizeof(long) * store->pixelids_size * 2);
            store->pixelids_size++;
        }
        store->pixelids[store->npixels] = spl->pix_nest;
        store->npixels++;
    }

    pix = pixels[index];

    if (pix->size == pix->nsamples) { /* need realloc */
        pix->samples = REALLOC(pix->samples, sizeof(Sample*) * pix->size * 2);
        pix->size *= 2;
    }

    /* append sample */
    pix->samples[pix->nsamples] = spl;
    pix->nsamples++;

}

static void
insert_sample_into_avltree_store(PixelStore *store, Sample *spl, long nsides) {

    /* search for the pixel */
    pixel_avl *avlpix =
            pixelAvlSearch((pixel_avl*) store->pixels, spl->pix_nest);

    if (!avlpix) { // no such pixel

        /* allocate and initialize */
        avlpix = CALLOC(1, sizeof(pixel_avl));
        avlpix->pixel.id = spl->pix_nest;
        avlpix->pixel.samples = ALLOC(sizeof(Sample**) * SPL_BASE_SIZE);
        avlpix->pixel.nsamples = 0;
        avlpix->pixel.size = SPL_BASE_SIZE;
        neighbours_nest(nsides, spl->pix_nest, avlpix->pixel.neighbors);

        /* insert new pixel */
        pixelAvlInsert((pixel_avl**) &store->pixels, avlpix);

        /* update npixels and array of pixelids store */
        if (store->pixelids_size == store->npixels) {
            store->pixelids = REALLOC(store->pixelids, sizeof(long) * store->pixelids_size * 2);
            store->pixelids_size *= 2;
        }
        store->pixelids[store->npixels] = spl->pix_nest;
        store->npixels++;

    }

    /* Insert sample in HealPixel */
    HealPixel *pix = &avlpix->pixel;
    if (pix->nsamples == pix->size) {
        /* need realloc */
        pix->samples = REALLOC(pix->samples, sizeof(Sample**) * pix->size * 2);
        pix->size *= 2;
    }

    pix->samples[pix->nsamples] = spl;
    pix->nsamples++;

}

#define PIXELIDS_BASE_SIZE 1000
static PixelStore*
new_store() {

    PixelStore *store = ALLOC(sizeof(PixelStore));

    store->pixels = NULL;
    store->npixels = 0;
    store->pixelids = ALLOC(sizeof(long) * PIXELIDS_BASE_SIZE);
    store->pixelids_size = PIXELIDS_BASE_SIZE;

    return store;
}

static PixelStore*
new_store_bigarray(Field *fields, int nfields, long nsides) {
    PixelStore *store = new_store();
    store->scheme = STORE_SCHEME_BIGARRAY;

    /* Allocate room for all possible pixels */
    long npix = nside2npix(nsides);

    Logger_log(LOGGER_NORMAL,
            "Will allocate room for %li pixels. It will take %i MB\n",
            npix, sizeof(HealPixel*) * npix / 1000000);

    store->pixels = (HealPixel**) CALLOC(npix, sizeof(HealPixel*));

    /* fill pixels with samples values */
    long i;
    int j, k;
    Field *field;
    Set *set;
    Sample *spl;

    long total_nsamples;
    total_nsamples = 0;

    for (i=0; i<nfields; i++) {
        field = &fields[i];

        for (j=0; j<field->nsets; j++) {
            set = &field->sets[j];

            total_nsamples += set->nsamples;
            for (k=0; k<set->nsamples; k++) {

                spl = &set->samples[k];
                spl->bestMatch = NULL;

                ang2pix_nest(nsides, spl->dec, spl->ra, &spl->pix_nest);
                ang2vec(spl->dec, spl->ra, spl->vector);
                insert_sample_into_bigarray_store(spl, store, spl->pix_nest, nsides);

            }
        }
    }

    Logger_log(LOGGER_TRACE,
            "Total size for pixels is %li MB\n",
            (nside2npix(nsides) * sizeof(HealPixel*) +
                    total_nsamples * sizeof(Sample)) / 1000000);

    return store;

}

static PixelStore*
new_store_avltree(Field *fields, int nfields, long nsides) {

    PixelStore *store = new_store();
    store->scheme = STORE_SCHEME_AVLTREE;

    Field field;
    Set set;
    Sample *spl;

    int i, j, k;
    for (i = 0; i < nfields; i++) {
        field = fields[i];

        for (j = 0; j < field.nsets; j++) {
            set = field.sets[j];

            for (k = 0; k < set.nsamples; k++) {

                spl = &set.samples[k];
                spl->bestMatch = NULL;
                ang2pix_nest(nsides,spl->dec, spl->ra,&spl->pix_nest);
                ang2vec(spl->dec, spl->ra, spl->vector);
                insert_sample_into_avltree_store(store, spl, nsides);

            }
        }
    }

    return store;
}

static void
free_store_bigarray(PixelStore *store) {
    long i;
    long *pixids = store->pixelids;
    long npix = store->npixels;
    HealPixel **pixels = (HealPixel**) store->pixels;

    for (i=0; i<npix; i++) {
        FREE(pixels[pixids[i]]->samples);
    }
    FREE(pixels);
}
/**
 * PRIVATE FUNCTIONS END
 ******************************************************************************/



/******************************************************************************
 * PUBLIC FUNCTIONS
 */
PixelStore*
PixelStore_new(Field *fields, int nfields, long nsides, StoreScheme scheme) {
    switch(scheme) {
    case STORE_SCHEME_BIGARRAY:
        return new_store_bigarray(fields, nfields, nsides);
    case STORE_SCHEME_AVLTREE:
        return new_store_avltree(fields, nfields, nsides);
    default:
        return NULL;
    }
}

HealPixel*
PixelStore_get(PixelStore* store, long key) {
    pixel_avl *match_avl;
    HealPixel **match_big;

    switch(store->scheme) {
    case STORE_SCHEME_AVLTREE:
        match_avl = pixelAvlSearch((pixel_avl*) store->pixels, key);
        if (!match_avl)
            return (HealPixel*) NULL;
        return &match_avl->pixel;
    case STORE_SCHEME_BIGARRAY:
        match_big = (HealPixel**) store->pixels;
        return match_big[key];
    default:
        return NULL;
    }
}

void
PixelStore_free(PixelStore* store) {
    switch(store->scheme) {
    case STORE_SCHEME_AVLTREE:
        pixelAvlFree((pixel_avl*) store->pixels);
        FREE(store);
        return;
    case STORE_SCHEME_BIGARRAY:
        free_store_bigarray(store);
        FREE(store);
        return;
    }
}
/**
 * PUBLIC FUNCTIONS END
 ******************************************************************************/

