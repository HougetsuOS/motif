/*
 * XmPlatDrawCairo.c - the cairo-Xlib render implementation of the
 * Phase-1 draw-primitive contract (doc/plat-abstraction.md §3 Phase 6).
 *
 * Scope: the 14 vector primitives plus the two special fills.  Everything
 * else in the contract (attribute setters, GC readback, text, images,
 * blits, events, atoms) is shared with the core-Xlib file and lives in
 * XmPlatDraw.c; both render variants link into the same backend object.
 *
 * Attribute model: the GC that rides in the draw context remains the
 * single source of truth.  Each prim reads the GC state it needs
 * (XGetGCValues), mirrors it into cairo state, and draws.  This keeps
 * widget GC fields (frozen API) meaningful and lets the shared Get*
 * readbacks and the core-Xlib fallback stay consistent.
 *
 * Cairo cannot reproduce the X11 raster operators: when the GC function
 * is not GXcopy (drag-preview XOR drawing) the prim falls back to the
 * core-Xlib path on the same drawable.  The same escape applies when the
 * GC carries a clip-mask pixmap (cairo has no direct equivalent; the
 * sites are rare: IconG/TabBox icons).
 *
 * When XMPLAT_CAIRO_RENDER is undefined this file compiles empty and the
 * core-Xlib implementation in XmPlatDraw.c provides the primitives.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef XMPLAT_CAIRO_RENDER

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <math.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include "XmPlatP.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- helpers --------------------------------------------------------- */

/*
 * The cairo context for a draw ctx, created on first use against the
 * destination drawable.  Cairo surfaces are cached per (ctx) lifetime
 * (the ctx is a per-draw-block object created by _XmPlatCtx; the surface
 * cache of the Xlib side has no equivalent here).
 */
static cairo_t *
CairoOf (XmPlatDrawCtx ctx)
{
    if (ctx->cr == NULL) {
	cairo_surface_t *s ;

	s = cairo_xlib_surface_create (ctx->dpy, ctx->surface->d,
				       DefaultVisual (ctx->dpy,
						      DefaultScreen (ctx->dpy)),
				       0, 0) ;
	ctx->cr = s ;
    }
    return (cairo_t *) ctx->cr ;
}

/* Read the GC's foreground/background as cairo rgb (0..1). */
static void
GcColor (XmPlatDrawCtx ctx, unsigned long which, double *r, double *g,
	 double *b)
{
    XGCValues v ;
    XColor xcol ;

    XGetGCValues (ctx->dpy, ctx->gc, which, &v) ;
    xcol.pixel = (which == GCForeground) ? v.foreground : v.background ;
    XQueryColor (ctx->dpy, DefaultColormap (ctx->dpy, DefaultScreen (ctx->dpy)),
		 &xcol) ;
    *r = xcol.red   / 65535.0 ;
    *g = xcol.green / 65535.0 ;
    *b = xcol.blue  / 65535.0 ;
}

/*
 * Per-prim state mirror: source color, line width/dash, clip.  Returns
 * the cairo_t to draw on, or NULL when the prim must take the core-Xlib
 * fallback (non-GXcopy function, clip-mask pixmap, stippled/tiled GC
 * fills are handled by the special fills instead).
 */
static cairo_t *
PrimState (XmPlatDrawCtx ctx, int need_line)
{
    cairo_t *cr ;
    XGCValues v ;
    static const double dash_on[1]  = { 4.0 } ;
    static const double dash_off[2] = { 2.0, 4.0 } ;

    XGetGCValues (ctx->dpy, ctx->gc,
		  GCFunction | GCLineWidth | GCLineStyle | GCCapStyle |
		  GCJoinStyle | GCClipMask | GCClipXOrigin | GCClipYOrigin |
		  GCFillStyle, &v) ;

    /* raster-operator and clip-mask fallbacks (see file header) */
    if (v.function != GXcopy) return NULL ;
    if (v.clip_mask != None)  return NULL ;

    cr = CairoOf (ctx) ;
    if (cr == NULL) return NULL ;

    /* source = GC foreground */
    {
	double r, g, b ;
	GcColor (ctx, GCForeground, &r, &g, &b) ;
	cairo_set_source_rgb (cr, r, g, b) ;
    }

    /* Clip: _XmPlatSetClipRect (shared) has already mirrored the clip
       into the cairo context at set time; nothing to do per-prim. */

    cairo_set_operator (cr, CAIRO_OPERATOR_OVER) ;

    if (need_line) {
	double dash_offset ;
	cairo_set_line_width (cr, v.line_width ? v.line_width : 1.0) ;
	cairo_set_line_cap (cr,
			    v.cap_style == CapRound ? CAIRO_LINE_CAP_ROUND :
			    v.cap_style == CapProjecting ?
			      CAIRO_LINE_CAP_SQUARE : CAIRO_LINE_CAP_BUTT) ;
	cairo_set_line_join (cr,
			     v.join_style == JoinRound ? CAIRO_LINE_JOIN_ROUND :
			     v.join_style == JoinBevel ?
			       CAIRO_LINE_JOIN_BEVEL : CAIRO_LINE_JOIN_MITER) ;
	dash_offset = ctx->dash_offset ;
	if (v.line_style == LineOnOffDash) {
	    if (ctx->ndash > 0) {
		double pats[18] ;
		int i, np = ctx->ndash < 18 ? ctx->ndash : 18 ;
		for (i = 0 ; i < np ; i++)
		    pats[i] = ctx->dash[i] ? ctx->dash[i] : 1.0 ;
		cairo_set_dash (cr, pats, np, dash_offset) ;
	    } else {
		cairo_set_dash (cr,
				v.line_style == LineDoubleDash ?
				  dash_off : dash_on, 2, dash_offset) ;
	    }
	} else {
	    cairo_set_dash (cr, NULL, 0, 0.0) ;
	}
    }
    return cr ;
}

/* Clip handling: _XmPlatSetClipRect (shared file) records the rect on
   the ctx (ndash misuse avoided: dedicated fields). */
void
_XmPlatCairoCtxInit (XmPlatDrawCtx c)
{
    c->cr = NULL ;
    c->ndash = 0 ;
    c->dash_offset = 0 ;
}

void
_XmPlatCairoCtxFini (XmPlatDrawCtx c)
{
    if (c && c->cr != NULL) {
	cairo_destroy ((cairo_t *) c->cr) ;
	c->cr = NULL ;
    }
}

void
_XmPlatCairoCtxDashes (XmPlatDrawCtx c, const unsigned char *l, int n,
		       unsigned int off)
{
    int i ;

    if (c == NULL) return ;
    c->ndash = (n < 36) ? n : 36 ;
    for (i = 0 ; i < c->ndash ; i++) c->dash[i] = l[i] ;
    c->dash_offset = (double) off ;
}

/* Clip recording: the shared _XmPlatSetClipRect calls this (weak hook —
   defined here only under the cairo build). */
void
_XmPlatCairoClipRect (XmPlatDrawCtx ctx, int x, int y,
		      unsigned int w, unsigned int h)
{
    cairo_t *cr ;

    if (ctx == NULL || ctx->surface == NULL) return ;
    /* Mirror only into an existing cairo context: several sites build a
       GC-only ctx (drawable None) just to push clip state into the GC
       for later use; there is nothing to mirror there. */
    if (ctx->cr == NULL) return ;
    if (ctx->surface->d == None) return ;
    cr = (cairo_t *) ctx->cr ;
    cairo_reset_clip (cr) ;
    if (w != 0 && h != 0) {
	cairo_rectangle (cr, x, y, w, h) ;
	cairo_clip (cr) ;
    }
}

/* ---- the 14 vector primitives ---------------------------------------- */

void
_XmPlatDrawPoint (XmPlatDrawCtx ctx, int x, int y)
{
    cairo_t *cr = PrimState (ctx, 0) ;

    if (cr == NULL) {
	XDrawPoint (ctx->dpy, ctx->surface->d, ctx->gc, x, y) ;
	return ;
    }
    cairo_rectangle (cr, x, y, 1.0, 1.0) ;
    cairo_fill (cr) ;
}

void
_XmPlatDrawPoints (XmPlatDrawCtx ctx, const XmPlatPoint *pts, int npts,
		   int relative)
{
    cairo_t *cr = PrimState (ctx, 0) ;
    int i, ox = 0, oy = 0 ;

    if (cr == NULL) {
	XPoint *xpts = (XPoint *) XtMalloc ((size_t)npts * sizeof (XPoint)) ;
	for (i = 0 ; i < npts ; i++) {
	    xpts[i].x = (short)pts[i].x ;
	    xpts[i].y = (short)pts[i].y ;
	}
	XDrawPoints (ctx->dpy, ctx->surface->d, ctx->gc, xpts, npts,
		     relative? CoordModePrevious : CoordModeOrigin) ;
	XtFree ((char *) xpts) ;
	return ;
    }
    for (i = 0 ; i < npts ; i++) {
	int px = pts[i].x + (relative ? ox : 0) ;
	int py = pts[i].y + (relative ? oy : 0) ;
	cairo_rectangle (cr, px, py, 1.0, 1.0) ;
	ox = relative ? px : 0 ;
	oy = relative ? py : 0 ;
    }
    cairo_fill (cr) ;
}

void
_XmPlatDrawLine (XmPlatDrawCtx ctx, int x1, int y1, int x2, int y2)
{
    cairo_t *cr = PrimState (ctx, 1) ;

    if (cr == NULL) {
	XDrawLine (ctx->dpy, ctx->surface->d, ctx->gc, x1, y1, x2, y2) ;
	return ;
    }
    cairo_new_path (cr) ;
    cairo_move_to (cr, x1 + 0.5, y1 + 0.5) ;
    cairo_line_to (cr, x2 + 0.5, y2 + 0.5) ;
    cairo_stroke (cr) ;
}

void
_XmPlatDrawLines (XmPlatDrawCtx ctx, const XmPlatPoint *pts, int npts,
		  int relative)
{
    cairo_t *cr = PrimState (ctx, 1) ;
    int i, ox = 0, oy = 0 ;

    if (cr == NULL) {
	XPoint *xpts = (XPoint *) XtMalloc ((size_t)npts * sizeof (XPoint)) ;
	for (i = 0 ; i < npts ; i++) {
	    xpts[i].x = (short)pts[i].x ;
	    xpts[i].y = (short)pts[i].y ;
	}
	XDrawLines (ctx->dpy, ctx->surface->d, ctx->gc, xpts, npts,
		    relative? CoordModePrevious : CoordModeOrigin) ;
	XtFree ((char *) xpts) ;
	return ;
    }
    cairo_new_path (cr) ;
    for (i = 0 ; i < npts ; i++) {
	int px = pts[i].x + (relative ? ox : 0) ;
	int py = pts[i].y + (relative ? oy : 0) ;
	if (i == 0) cairo_move_to (cr, px + 0.5, py + 0.5) ;
	else cairo_line_to (cr, px + 0.5, py + 0.5) ;
	ox = relative ? px : 0 ;
	oy = relative ? py : 0 ;
    }
    cairo_stroke (cr) ;
}

void
_XmPlatDrawSegments (XmPlatDrawCtx ctx, const XmPlatSegment *segs, int nsegs)
{
    cairo_t *cr = PrimState (ctx, 1) ;
    int i ;

    if (cr == NULL) {
	XSegment *xsegs = (XSegment *) XtMalloc ((size_t)nsegs * sizeof (XSegment)) ;
	for (i = 0 ; i < nsegs ; i++) {
	    xsegs[i].x1 = (short)segs[i].x1 ;
	    xsegs[i].y1 = (short)segs[i].y1 ;
	    xsegs[i].x2 = (short)segs[i].x2 ;
	    xsegs[i].y2 = (short)segs[i].y2 ;
	}
	XDrawSegments (ctx->dpy, ctx->surface->d, ctx->gc, xsegs, nsegs) ;
	XtFree ((char *) xsegs) ;
	return ;
    }
    cairo_new_path (cr) ;
    for (i = 0 ; i < nsegs ; i++) {
	cairo_move_to (cr, segs[i].x1 + 0.5, segs[i].y1 + 0.5) ;
	cairo_line_to (cr, segs[i].x2 + 0.5, segs[i].y2 + 0.5) ;
    }
    cairo_stroke (cr) ;
}

void
_XmPlatDrawRect (XmPlatDrawCtx ctx, int x, int y, unsigned int w,
		 unsigned int h)
{
    cairo_t *cr = PrimState (ctx, 1) ;

    if (cr == NULL) {
	XDrawRectangle (ctx->dpy, ctx->surface->d, ctx->gc, x, y, w, h) ;
	return ;
    }
    cairo_new_path (cr) ;
    cairo_rectangle (cr, x + 0.5, y + 0.5, w, h) ;
    cairo_stroke (cr) ;
}

void
_XmPlatDrawRects (XmPlatDrawCtx ctx, const XmPlatRect *rects, int nrects)
{
    cairo_t *cr = PrimState (ctx, 1) ;
    int i ;

    if (cr == NULL) {
	XRectangle *xr = (XRectangle *)
			    XtMalloc ((size_t)nrects * sizeof (XRectangle)) ;
	for (i = 0 ; i < nrects ; i++) {
	    xr[i].x = (short)rects[i].x ;
	    xr[i].y = (short)rects[i].y ;
	    xr[i].width = (unsigned short)rects[i].width ;
	    xr[i].height = (unsigned short)rects[i].height ;
	}
	XDrawRectangles (ctx->dpy, ctx->surface->d, ctx->gc, xr, nrects) ;
	XtFree ((char *) xr) ;
	return ;
    }
    cairo_new_path (cr) ;
    for (i = 0 ; i < nrects ; i++)
	cairo_rectangle (cr, rects[i].x + 0.5, rects[i].y + 0.5,
			 rects[i].width, rects[i].height) ;
    cairo_stroke (cr) ;
}

void
_XmPlatDrawArc (XmPlatDrawCtx ctx, int x, int y, unsigned int w,
		unsigned int h, int angle1, int angle2)
{
    cairo_t *cr = PrimState (ctx, 1) ;
    double a1, a2 ;

    if (cr == NULL) {
	XDrawArc (ctx->dpy, ctx->surface->d, ctx->gc, x, y, w, h,
		  angle1, angle2) ;
	return ;
    }
    a1 = angle1 / 64.0 ;
    a2 = angle2 / 64.0 ;
    cairo_new_path (cr) ;
    cairo_save (cr) ;
    cairo_translate (cr, x + w / 2.0, y + h / 2.0) ;
    cairo_scale (cr, w / 2.0, h / 2.0) ;
    /* X angles start at 3 o'clock going clockwise; cairo likewise
       (positive = clockwise in the flipped y). */
    cairo_arc (cr, 0.0, 0.0, 1.0, -a1 * M_PI / 180.0,
	       -(a1 + a2) * M_PI / 180.0) ;
    cairo_restore (cr) ;
    cairo_stroke (cr) ;
}

void
_XmPlatFillRect (XmPlatDrawCtx ctx, int x, int y, unsigned int w,
		 unsigned int h)
{
    cairo_t *cr = PrimState (ctx, 0) ;

    if (cr == NULL) {
	XFillRectangle (ctx->dpy, ctx->surface->d, ctx->gc, x, y, w, h) ;
	return ;
    }
    cairo_new_path (cr) ;
    cairo_rectangle (cr, x, y, w, h) ;
    cairo_fill (cr) ;
}

void
_XmPlatFillRects (XmPlatDrawCtx ctx, const XmPlatRect *rects, int nrects)
{
    cairo_t *cr = PrimState (ctx, 0) ;
    int i ;

    if (cr == NULL) {
	XRectangle *xr = (XRectangle *)
			    XtMalloc ((size_t)nrects * sizeof (XRectangle)) ;
	for (i = 0 ; i < nrects ; i++) {
	    xr[i].x = (short)rects[i].x ;
	    xr[i].y = (short)rects[i].y ;
	    xr[i].width = (unsigned short)rects[i].width ;
	    xr[i].height = (unsigned short)rects[i].height ;
	}
	XFillRectangles (ctx->dpy, ctx->surface->d, ctx->gc, xr, nrects) ;
	XtFree ((char *) xr) ;
	return ;
    }
    cairo_new_path (cr) ;
    for (i = 0 ; i < nrects ; i++)
	cairo_rectangle (cr, rects[i].x, rects[i].y,
			 rects[i].width, rects[i].height) ;
    cairo_fill (cr) ;
}

void
_XmPlatFillPolygon (XmPlatDrawCtx ctx, const XmPlatPoint *pts, int npts,
		    int convex)
{
    cairo_t *cr = PrimState (ctx, 0) ;
    int i ;

    (void) convex ;
    if (cr == NULL) {
	XPoint *xpts = (XPoint *) XtMalloc ((size_t)npts * sizeof (XPoint)) ;
	for (i = 0 ; i < npts ; i++) {
	    xpts[i].x = (short)pts[i].x ;
	    xpts[i].y = (short)pts[i].y ;
	}
	XFillPolygon (ctx->dpy, ctx->surface->d, ctx->gc, xpts, npts,
		      convex? Convex : Nonconvex, CoordModeOrigin) ;
	XtFree ((char *) xpts) ;
	return ;
    }
    cairo_new_path (cr) ;
    cairo_move_to (cr, pts[0].x + 0.5, pts[0].y + 0.5) ;
    for (i = 1 ; i < npts ; i++)
	cairo_line_to (cr, pts[i].x + 0.5, pts[i].y + 0.5) ;
    cairo_close_path (cr) ;
    cairo_fill (cr) ;
}

void
_XmPlatFillArc (XmPlatDrawCtx ctx, int x, int y, unsigned int w,
		unsigned int h, int angle1, int angle2)
{
    cairo_t *cr = PrimState (ctx, 0) ;
    double a1, a2 ;

    if (cr == NULL) {
	XFillArc (ctx->dpy, ctx->surface->d, ctx->gc, x, y, w, h,
		  angle1, angle2) ;
	return ;
    }
    a1 = angle1 / 64.0 ;
    a2 = angle2 / 64.0 ;
    cairo_new_path (cr) ;
    cairo_save (cr) ;
    cairo_translate (cr, x + w / 2.0, y + h / 2.0) ;
    cairo_scale (cr, w / 2.0, h / 2.0) ;
    cairo_arc (cr, 0.0, 0.0, 1.0, -a1 * M_PI / 180.0,
	       -(a1 + a2) * M_PI / 180.0) ;
    cairo_restore (cr) ;
    cairo_close_path (cr) ;
    cairo_fill (cr) ;
}

/* ---- special fills ---------------------------------------------------- */

void
_XmPlatFillRectangleTiled (XmPlatDrawCtx ctx, int x, int y,
			   unsigned int w, unsigned int h)
{
    /* Pixmap-pattern fill stays on core X: cairo xlib-surfaces of
       1-bit/deep pixmaps need the pixmap's exact visual, and these fills
       are rare (tab decorations).  The vector prims above carry the
       cairo work. */
    XGCValues v ;
    unsigned long mask = GCFillStyle ;

    XGetGCValues (ctx->dpy, ctx->gc, GCFunction | GCClipMask, &v) ;
    if (v.function != GXcopy || v.clip_mask != None) {
	v.fill_style = FillTiled ;
	XChangeGC (ctx->dpy, ctx->gc, mask, &v) ;
	XFillRectangle (ctx->dpy, ctx->surface->d, ctx->gc, x, y, w, h) ;
	v.fill_style = FillSolid ;
	XChangeGC (ctx->dpy, ctx->gc, mask, &v) ;
	return ;
    }
    v.fill_style = FillTiled ;
    XChangeGC (ctx->dpy, ctx->gc, mask, &v) ;
    XFillRectangle (ctx->dpy, ctx->surface->d, ctx->gc, x, y, w, h) ;
    v.fill_style = FillSolid ;
    XChangeGC (ctx->dpy, ctx->gc, mask, &v) ;
}

void
_XmPlatFillRectangleStippled (XmPlatDrawCtx ctx, int x, int y,
			      unsigned int w, unsigned int h)
{
    XGCValues v ;
    unsigned long mask = GCFillStyle ;

    v.fill_style = FillStippled ;
    XChangeGC (ctx->dpy, ctx->gc, mask, &v) ;
    XFillRectangle (ctx->dpy, ctx->surface->d, ctx->gc, x, y, w, h) ;
    v.fill_style = FillSolid ;
    XChangeGC (ctx->dpy, ctx->gc, mask, &v) ;
}

#endif /* XMPLAT_CAIRO_RENDER */