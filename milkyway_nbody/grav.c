/* ************************************************************************** */
/* GRAV.C: routines to compute gravity. Public routines: hackgrav(). */
/* */
/* Copyright (c) 1993 by Joshua E. Barnes, Honolulu, HI. */
/* It's free because it's yours. */
/* ************************************************************************** */

#include "nbody.h"
#include "util.h"
#include <stdio.h>

static bool treescan(const NBodyCtx*, NBodyState*, nodeptr);           /* does force calculation */
static bool subdivp(cellptr);            /* can cell be accepted? */
static void gravsub(const NBodyCtx*, nodeptr);            /* compute grav interaction */

/* HACKGRAV: evaluate gravitational field on body p; checks to be
 * sure self-interaction was handled correctly if intree is true.
 */

static bodyptr pskip;                /* skip in force evaluation */
static vector pos0;              /* point to evaluate field */
static real phi0;                /* resulting potential */
static vector acc0;              /* resulting acceleration */


void hackgrav(const NBodyCtx* ctx, NBodyState* st, bodyptr p, bool intree)
{
    vector externalacc;
    int n2bterm;    /* number 2-body of terms evaluated */
    int nbcterm;    /* num of body-cell terms evaluated */
    static bool treeincest = FALSE;     /* tree-incest occured */
    bool skipself          = FALSE;     /* self-interaction skipped */

    pskip = p;                  /* exclude p from f.c. */
    SETV(pos0, Pos(p));             /* set field point */
    phi0 = 0.0;                 /* init total potential */
    CLRV(acc0);                 /* and total acceleration */
    n2bterm = nbcterm = 0;          /* count body & cell terms */
                  /* watch for tree-incest */
    skipself = treescan(ctx, st, (nodeptr) st->tree.root);           /* scan tree from t.root */
    if (intree && !skipself)            /* did tree-incest occur? */
    {
        if (!ctx->allowIncest) /* treat as catastrophic? */
            error("hackgrav: tree-incest detected\n");
        if (!treeincest)           /* for the first time? */
            eprintf("\n[hackgrav: tree-incest detected]\n");
        treeincest = TRUE;          /* don't repeat warning */
    }

    /* Adding the external potential */
    acceleration(ctx, Pos(p), externalacc);

    INCADDV(acc0, externalacc);

    /* TODO: Sharing */
    /* CHECKME: Only the acceleration here seems to have an effect on
     * the results */

    phi0 -=   miyamotoNagaiPhi(&ctx->pot.disk, Pos(p))
            + sphericalPhi(&ctx->pot.sphere[0], Pos(p))
            + logHaloPhi(&ctx->pot.halo, Pos(p));

    Phi(p) = phi0;              /* store total potential */
    SETV(Acc(p), acc0);         /* and acceleration */

}

/* treescan: iterative routine to do force calculation, starting with
 * node q, which is typically the t.root cell. Watches for tree
 * incest.
 */
static bool treescan(const NBodyCtx* ctx, NBodyState* st, nodeptr q)
{
    bool skipself = FALSE;

    while (q != NULL)               /* while not at end of scan */
    {
        if (Type(q) == CELL &&          /* is node a cell and... */
                subdivp((cellptr) q))       /* too close to accept? */
            q = More(q);            /* follow to next level */
        else                    /* else accept this term */
        {
            if (q == (nodeptr) pskip)       /* self-interaction? */
                skipself = TRUE;        /* then just skip it */
            else                /* not self-interaction */
            {
                gravsub(ctx, q);                     /* so compute gravity */
                if (Type(q) == BODY)
                    st->n2bterm++;          /* count body-body */
                else
                    st->nbcterm++;          /* count body-cell */
            }
            q = Next(q);            /* follow next link */
        }
    }

    return skipself;
}

/*  * SUBDIVP: decide if cell q is too close to accept as a single
 * term.  Also sets qmem, dr, and drsq for use by gravsub.
 */

static cellptr qmem;                         /* data shared with gravsub */
static vector dr;                /* vector from q to pos0 */
static real drsq;                /* squared distance to pos0 */

static bool subdivp(cellptr q)
{
    SUBV(dr, Pos(q), pos0);         /* compute displacement */
    SQRV(drsq, dr);                 /* and find dist squared */
    qmem = q;                       /* remember we know them */
    return (drsq < Rcrit2(q));      /* apply standard rule */
}

/*  * GRAVSUB: compute contribution of node q to gravitational field at
 * point pos0, and add to running totals phi0 and acc0.
 */

static void gravsub(const NBodyCtx* ctx, nodeptr q)
{
    real drab, phii, mor3;
    vector ai, quaddr;
    real dr5inv, phiquad, drquaddr;

    if (q != (nodeptr) qmem)                    /* cant use memorized data? */
    {
        SUBV(dr, Pos(q), pos0);                 /* then compute sep. */
        SQRV(drsq, dr);                         /* and sep. squared */
    }
    drsq += ctx->model.eps * ctx->model.eps;          /* use standard softening */
    drab = rsqrt(drsq);
    phii = Mass(q) / drab;
    mor3 = phii / drsq;
    MULVS(ai, dr, mor3);
    phi0 -= phii;                               /* add to total grav. pot. */
    INCADDV(acc0, ai);                          /* ... and to total accel. */

    if (ctx->usequad && Type(q) == CELL)             /* if cell, add quad term */
    {
        dr5inv = 1.0 / (drsq * drsq * drab);    /* form dr^-5 */
        MULMV(quaddr, Quad(q), dr);             /* form Q * dr */
        DOTVP(drquaddr, dr, quaddr);            /* form dr * Q * dr */
        phiquad = -0.5 * dr5inv * drquaddr;     /* get quad. part of phi */
        phi0 += phiquad;                        /* increment potential */
        phiquad = 5.0 * phiquad / drsq;         /* save for acceleration */
        MULVS(ai, dr, phiquad);                 /* components of acc. */
        INCSUBV(acc0, ai);                      /* increment */
        INCMULVS(quaddr, dr5inv);
        INCSUBV(acc0, quaddr);                  /* acceleration */
    }
}

