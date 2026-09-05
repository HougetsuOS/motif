/*
 * XmPlat.c - lifecycle glue for the platform layer.
 *
 * Phase 1: surface/ctx handles are allocated eagerly at the seam and
 * freed by the holder.  A real handle cache comes with Phase 2 when
 * fonts join the contract.
 */
#include "XmPlatP.h"

void
_XmPlatSurfaceFree (XmPlatSurface surface)
{
    if (surface) XtFree ((char *) surface) ;
}

void
_XmPlatDrawCtxFree (XmPlatDrawCtx ctx)
{
    if (ctx) XtFree ((char *) ctx) ;
}
