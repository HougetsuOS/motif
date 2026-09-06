/*
 * XmPlatDraw.c - the X11 (core Xlib) implementation of the Phase-1
 * draw-primitive contract (doc/plat-abstraction.md §2.1, §3 Phase 1).
 *
 * Every function is a thin translation layer: contract call -> Xlib
 * call.  Attribute setters write through to the GC immediately (the
 * cache in XmPlatP.h exists for future batching, not for semantics).
 *
 * When Phase 6 lands, this file is REPLACED by a cairo-Xlib
 * implementation with identical entry points; callers never change.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "XmPlatP.h"

/* The X11 backend stores the XImage pointer directly in the token. */
struct _XmPlatImageRec {
    XImage *ximage ;
} ;

/* --- construction (migration seam) --------------------------------- */

XmPlatSurface
_XmPlatSurfaceOf (Display *dpy, Drawable d, Visual *visual, int depth,
		  Window window)
{
    XmPlatSurface s = (XmPlatSurface) XtMalloc (sizeof (struct _XmPlatSurfaceRec)) ;

    s->dpy = dpy ;
    s->d = d ;
    s->visual = visual ;
    s->depth = depth ;
    s->window = window ;
    return s ;
}

XmPlatDrawCtx
_XmPlatDrawCtxOf (Display *dpy, GC gc)
{
    XmPlatDrawCtx c = (XmPlatDrawCtx) XtMalloc (sizeof (struct _XmPlatDrawCtxRec)) ;

    c->dpy = dpy ;
    c->gc = gc ;
    c->surface = NULL ;
    c->cached_mask = 0 ;
    c->cr = NULL ;
    c->ndash = 0 ;
    c->dash_offset = 0 ;
    c->mem = 0 ; c->mem_line_width = 0 ; c->mem_line_style = 0 ;
    c->mem_cap = 0 ; c->mem_join = 0 ; c->mem_clip_on = 0 ;
#ifdef XMPLAT_CAIRO_RENDER
    _XmPlatCairoCtxInit (c) ;
#endif
    return c ;
}

Drawable
_XmPlatSurfaceDrawable (XmPlatSurface surface)
{
    return surface->d ;
}

/* --- attribute setters --------------------------------------------- */

void
_XmPlatSetForeground (XmPlatDrawCtx ctx, XmPlatPixel fg)
{
#ifdef XMPLAT_CAIRO_RENDER
    if (ctx->gc == NULL) {
	/* memory mode: pixel is 0xRRGGBB */
	ctx->mem_fg[0] = ((fg >> 16) & 0xFF) / 255.0 ;
	ctx->mem_fg[1] = ((fg >>  8) & 0xFF) / 255.0 ;
	ctx->mem_fg[2] = ( fg        & 0xFF) / 255.0 ;
	return ;
    }
#endif
    XSetForeground (ctx->dpy, ctx->gc, fg) ;
}

void
_XmPlatSetLineWidth (XmPlatDrawCtx ctx, unsigned int width)
{
    XGCValues v ;
    unsigned long mask = GCLineWidth ;
#ifdef XMPLAT_CAIRO_RENDER
    if (ctx->gc == NULL) { ctx->mem_line_width = width ; return ; }
#endif
    v.line_width = (int)width ;
    XChangeGC (ctx->dpy, ctx->gc, mask, &v) ;
}

void
_XmPlatSetLineAttr (XmPlatDrawCtx ctx, const XmPlatLineAttr *la)
{
#ifdef XMPLAT_CAIRO_RENDER
    if (ctx->gc == NULL) {
	ctx->mem_line_width = la->width ;
	ctx->mem_line_style = la->style ;
	ctx->mem_cap = la->cap ;
	ctx->mem_join = la->join ;
	return ;
    }
#endif
    XSetLineAttributes (ctx->dpy, ctx->gc, (int) la->width,
			(la->style == XmPlatLineOnOffDash)?
			  LineOnOffDash :
			(la->style == XmPlatLineDoubleDash)?
			  LineDoubleDash : LineSolid,
			la->cap == 1? CapRound :
			la->cap == 2? CapProjecting : CapButt,
			la->join == 1? JoinRound :
			la->join == 2? JoinBevel : JoinMiter) ;
}

XmPlatLineAttr
_XmPlatGetLineAttr (XmPlatDrawCtx ctx)
{
    XGCValues v ;
    XmPlatLineAttr la ;

#ifdef XMPLAT_CAIRO_RENDER
    if (ctx->gc == NULL) {
	la.width = ctx->mem_line_width ;
	la.style = ctx->mem_line_style ;
	la.cap = ctx->mem_cap ;
	la.join = ctx->mem_join ;
	return la ;
    }
#endif
    XGetGCValues (ctx->dpy, ctx->gc,
		  GCLineWidth | GCLineStyle | GCCapStyle | GCJoinStyle, &v) ;
    la.width = (unsigned int) v.line_width ;
    la.style = (v.line_style == LineOnOffDash)? XmPlatLineOnOffDash :
	       (v.line_style == LineDoubleDash)? XmPlatLineDoubleDash :
	       XmPlatLineSolid ;
    la.cap  = (v.cap_style == CapRound)? 1 :
	      (v.cap_style == CapProjecting)? 2 : 0 ;
    la.join = (v.join_style == JoinRound)? 1 :
	      (v.join_style == JoinBevel)? 2 : 0 ;
    return la ;
}

unsigned int
_XmPlatGetLineWidth (XmPlatDrawCtx ctx)
{
    XGCValues v ;

#ifdef XMPLAT_CAIRO_RENDER
    if (ctx->gc == NULL) return ctx->mem_line_width ;
#endif
    XGetGCValues (ctx->dpy, ctx->gc, GCLineWidth, &v) ;
    return (unsigned int) v.line_width ;
}

void
_XmPlatSetLineStyle (XmPlatDrawCtx ctx, int style)
{
    XGCValues v ;
    unsigned long mask = GCLineStyle ;
#ifdef XMPLAT_CAIRO_RENDER
    if (ctx->gc == NULL) { ctx->mem_line_style = style ; return ; }
#endif
    v.line_style = (style == XmPlatLineOnOffDash)? LineOnOffDash :
		   (style == XmPlatLineDoubleDash)? LineDoubleDash : LineSolid ;
    XChangeGC (ctx->dpy, ctx->gc, mask, &v) ;
}

void
_XmPlatSetDashes (XmPlatDrawCtx ctx, const unsigned char *dash_list,
		  int ndash, unsigned int offset)
{
#ifdef XMPLAT_CAIRO_RENDER
    if (ctx->gc != NULL)
#endif
	XSetDashes (ctx->dpy, ctx->gc, (int)offset, (const char *) dash_list,
		    ndash) ;
#ifdef XMPLAT_CAIRO_RENDER
    _XmPlatCairoCtxDashes (ctx, dash_list, ndash, offset) ;
#endif
}

void
_XmPlatSetClipRect (XmPlatDrawCtx ctx, int x, int y,
		    unsigned int w, unsigned int h)
{
    XRectangle r ;

#ifdef XMPLAT_CAIRO_RENDER
    if (ctx->gc == NULL) {
	/* memory mode: mirror the clip into the ctx; the prim re-derives
	   it from the cairo clip (set below). */
	ctx->mem_clip_on = (w != 0 && h != 0) ;
	ctx->mem_clip_x = x ; ctx->mem_clip_y = y ;
	ctx->mem_clip_w = w ; ctx->mem_clip_h = h ;
	_XmPlatCairoClipRect (ctx, x, y, w, h) ;
	return ;
    }
#endif
    if (ctx->gc == NULL) return ;	/* Xft-draw-only ctx */
    if (w == 0 || h == 0) {
	XSetClipMask (ctx->dpy, ctx->gc, None) ;
#ifdef XMPLAT_CAIRO_RENDER
	_XmPlatCairoClipRect (ctx, 0, 0, 0, 0) ;
#endif
	return ;
    }
    r.x = (short)x ; r.y = (short)y ;
    r.width = (unsigned short)w ; r.height = (unsigned short)h ;
    XSetClipRectangles (ctx->dpy, ctx->gc, 0, 0, &r, 1, Unsorted) ;
#ifdef XMPLAT_CAIRO_RENDER
    _XmPlatCairoClipRect (ctx, x, y, w, h) ;
#endif
}

void
_XmPlatSetClipMask (XmPlatDrawCtx ctx, XmPlatSurface mask, int x, int y)
{
    XSetClipOrigin (ctx->dpy, ctx->gc, x, y) ;
    XSetClipMask (ctx->dpy, ctx->gc, mask->d) ;
}

void
_XmPlatFlush (XmPlatDrawCtx ctx)
{
#ifdef XMPLAT_CAIRO_RENDER
    if (ctx->gc == NULL) return ;	/* memory mode: nothing to flush */
#endif
    XFlush (ctx->dpy) ;
}

void
_XmPlatClearWindow (XmPlatSurface surface)
{
    XClearWindow (surface->dpy, surface->window) ;
}

void
_XmPlatSetBackground (XmPlatDrawCtx ctx, XmPlatPixel bg)
{
#ifdef XMPLAT_CAIRO_RENDER
    if (ctx->gc == NULL) {
	ctx->mem_bg[0] = ((bg >> 16) & 0xFF) / 255.0 ;
	ctx->mem_bg[1] = ((bg >>  8) & 0xFF) / 255.0 ;
	ctx->mem_bg[2] = ( bg        & 0xFF) / 255.0 ;
	return ;
    }
#endif
    XSetBackground (ctx->dpy, ctx->gc, bg) ;
}

XmPlatPixel
_XmPlatGetForeground (XmPlatDrawCtx ctx)
{
    XGCValues v ;

#ifdef XMPLAT_CAIRO_RENDER
    if (ctx->gc == NULL) {
	unsigned long r = (unsigned long) (ctx->mem_fg[0] * 255.0 + 0.5) ;
	unsigned long g = (unsigned long) (ctx->mem_fg[1] * 255.0 + 0.5) ;
	unsigned long b = (unsigned long) (ctx->mem_fg[2] * 255.0 + 0.5) ;
	return (r << 16) | (g << 8) | b ;
    }
#endif
    XGetGCValues (ctx->dpy, ctx->gc, GCForeground, &v) ;
    return v.foreground ;
}

XmPlatPixel
_XmPlatGetBackground (XmPlatDrawCtx ctx)
{
    XGCValues v ;

#ifdef XMPLAT_CAIRO_RENDER
    if (ctx->gc == NULL) {
	unsigned long r = (unsigned long) (ctx->mem_bg[0] * 255.0 + 0.5) ;
	unsigned long g = (unsigned long) (ctx->mem_bg[1] * 255.0 + 0.5) ;
	unsigned long b = (unsigned long) (ctx->mem_bg[2] * 255.0 + 0.5) ;
	return (r << 16) | (g << 8) | b ;
    }
#endif
    XGetGCValues (ctx->dpy, ctx->gc, GCBackground, &v) ;
    return v.background ;
}

unsigned long
_XmPlatGetFontId (XmPlatDrawCtx ctx)
{
    XGCValues v ;

    XGetGCValues (ctx->dpy, ctx->gc, GCFont, &v) ;
    return (unsigned long) v.font ;
}

XmPlatSurface
_XmPlatGetStipple (XmPlatDrawCtx ctx)
{
    XGCValues v ;

    XGetGCValues (ctx->dpy, ctx->gc, GCStipple, &v) ;
    if (v.stipple == None) return NULL ;
    return _XmPlatSurfaceOf (ctx->dpy, v.stipple, NULL, 0, None) ;
}

void
_XmPlatSetTile (XmPlatDrawCtx ctx, XmPlatSurface tile)
{
    XSetTile (ctx->dpy, ctx->gc, tile->d) ;
}

void
_XmPlatSetStipple (XmPlatDrawCtx ctx, XmPlatSurface stipple,
		   int ts_x_origin, int ts_y_origin)
{
    XSetStipple (ctx->dpy, ctx->gc, stipple->d) ;
    XSetTSOrigin (ctx->dpy, ctx->gc, ts_x_origin, ts_y_origin) ;
}

void
_XmPlatSetClipMaskSurf (XmPlatDrawCtx ctx, XmPlatSurface mask, int x, int y)
{
    XSetClipOrigin (ctx->dpy, ctx->gc, x, y) ;
    XSetClipMask (ctx->dpy, ctx->gc, mask->d) ;
}

void
_XmPlatChangeGCValues (XmPlatDrawCtx ctx, unsigned long value_mask,
		       const void *values)
{
    XChangeGC (ctx->dpy, ctx->gc, value_mask, (XGCValues *) values) ;
}

XmPlatDrawCtx
_XmPlatCreateCtxOnSurface (XmPlatSurface surface, unsigned long value_mask,
			   const void *values)
{
    GC gc = XCreateGC (surface->dpy, surface->d, value_mask,
		       (XGCValues *) values) ;
    return _XmPlatDrawCtxOf (surface->dpy, gc) ;
}

void
_XmPlatSetClipOrigin (XmPlatDrawCtx ctx, int x, int y)
{
    XSetClipOrigin (ctx->dpy, ctx->gc, x, y) ;
}

void
_XmPlatPutImage (XmPlatDrawCtx ctx, XmPlatImage image, int src_x, int src_y,
		 int dst_x, int dst_y, unsigned int w, unsigned int h)
{
    XPutImage (ctx->dpy, ctx->surface->d, ctx->gc, image->ximage,
	       src_x, src_y, dst_x, dst_y, w, h) ;
}

#ifndef XMPLAT_CAIRO_RENDER
/* --- draw primitives ----------------------------------------------- */

void
_XmPlatDrawPoint (XmPlatDrawCtx ctx, int x, int y)
{
    XDrawPoint (ctx->dpy, ctx->surface->d, ctx->gc, x, y) ;
}

void
_XmPlatDrawPoints (XmPlatDrawCtx ctx, const XmPlatPoint *pts, int npts,
		   int relative)
{
    XPoint *xpts = (XPoint *) XtMalloc ((size_t)npts * sizeof (XPoint)) ;
    int i ;

    for (i = 0 ; i < npts ; i++) {
	xpts[i].x = (short)pts[i].x ;
	xpts[i].y = (short)pts[i].y ;
    }
    XDrawPoints (ctx->dpy, ctx->surface->d, ctx->gc, xpts, npts,
		 relative? CoordModePrevious : CoordModeOrigin) ;
    XtFree ((char *) xpts) ;
}

void
_XmPlatDrawLine (XmPlatDrawCtx ctx, int x1, int y1, int x2, int y2)
{
    XDrawLine (ctx->dpy, ctx->surface->d, ctx->gc, x1, y1, x2, y2) ;
}

void
_XmPlatDrawLines (XmPlatDrawCtx ctx, const XmPlatPoint *pts, int npts,
		  int relative)
{
    XPoint *xpts = (XPoint *) XtMalloc ((size_t)npts * sizeof (XPoint)) ;
    int i ;

    for (i = 0 ; i < npts ; i++) {
	xpts[i].x = (short)pts[i].x ;
	xpts[i].y = (short)pts[i].y ;
    }
    XDrawLines (ctx->dpy, ctx->surface->d, ctx->gc, xpts, npts,
		relative? CoordModePrevious : CoordModeOrigin) ;
    XtFree ((char *) xpts) ;
}

void
_XmPlatDrawSegments (XmPlatDrawCtx ctx, const XmPlatSegment *segs, int nsegs)
{
    XSegment *xsegs = (XSegment *) XtMalloc ((size_t)nsegs * sizeof (XSegment)) ;
    int i ;

    for (i = 0 ; i < nsegs ; i++) {
	xsegs[i].x1 = (short)segs[i].x1 ;
	xsegs[i].y1 = (short)segs[i].y1 ;
	xsegs[i].x2 = (short)segs[i].x2 ;
	xsegs[i].y2 = (short)segs[i].y2 ;
    }
    XDrawSegments (ctx->dpy, ctx->surface->d, ctx->gc, xsegs, nsegs) ;
    XtFree ((char *) xsegs) ;
}

void
_XmPlatDrawRect (XmPlatDrawCtx ctx, int x, int y, unsigned int w, unsigned int h)
{
    XDrawRectangle (ctx->dpy, ctx->surface->d, ctx->gc, x, y, w, h) ;
}

void
_XmPlatDrawRects (XmPlatDrawCtx ctx, const XmPlatRect *rects, int nrects)
{
    XRectangle *xr = (XRectangle *) XtMalloc ((size_t)nrects * sizeof (XRectangle)) ;
    int i ;

    for (i = 0 ; i < nrects ; i++) {
	xr[i].x = (short)rects[i].x ;
	xr[i].y = (short)rects[i].y ;
	xr[i].width = (unsigned short)rects[i].width ;
	xr[i].height = (unsigned short)rects[i].height ;
    }
    XDrawRectangles (ctx->dpy, ctx->surface->d, ctx->gc, xr, nrects) ;
    XtFree ((char *) xr) ;
}

void
_XmPlatDrawArc (XmPlatDrawCtx ctx, int x, int y, unsigned int w, unsigned int h,
		int angle1, int angle2)
{
    XDrawArc (ctx->dpy, ctx->surface->d, ctx->gc, x, y, w, h, angle1, angle2) ;
}

void
_XmPlatFillRect (XmPlatDrawCtx ctx, int x, int y, unsigned int w, unsigned int h)
{
    XFillRectangle (ctx->dpy, ctx->surface->d, ctx->gc, x, y, w, h) ;
}

void
_XmPlatFillRects (XmPlatDrawCtx ctx, const XmPlatRect *rects, int nrects)
{
    XRectangle *xr = (XRectangle *) XtMalloc ((size_t)nrects * sizeof (XRectangle)) ;
    int i ;

    for (i = 0 ; i < nrects ; i++) {
	xr[i].x = (short)rects[i].x ;
	xr[i].y = (short)rects[i].y ;
	xr[i].width = (unsigned short)rects[i].width ;
	xr[i].height = (unsigned short)rects[i].height ;
    }
    XFillRectangles (ctx->dpy, ctx->surface->d, ctx->gc, xr, nrects) ;
    XtFree ((char *) xr) ;
}

void
_XmPlatFillPolygon (XmPlatDrawCtx ctx, const XmPlatPoint *pts, int npts,
		    int convex)
{
    XPoint *xpts = (XPoint *) XtMalloc ((size_t)npts * sizeof (XPoint)) ;
    int i ;

    for (i = 0 ; i < npts ; i++) {
	xpts[i].x = (short)pts[i].x ;
	xpts[i].y = (short)pts[i].y ;
    }
    XFillPolygon (ctx->dpy, ctx->surface->d, ctx->gc, xpts, npts,
		  convex? Convex : Nonconvex, CoordModeOrigin) ;
    XtFree ((char *) xpts) ;
}

void
_XmPlatFillArc (XmPlatDrawCtx ctx, int x, int y, unsigned int w, unsigned int h,
		int angle1, int angle2)
{
    XFillArc (ctx->dpy, ctx->surface->d, ctx->gc, x, y, w, h, angle1, angle2) ;
}

void
_XmPlatFillRectangleTiled (XmPlatDrawCtx ctx, int x, int y,
			   unsigned int w, unsigned int h)
{
    XGCValues v ;
    unsigned long mask = GCFillStyle ;

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
void
_XmPlatBlit (XmPlatDrawCtx ctx, XmPlatSurface src, int src_x, int src_y,
	     int dst_x, int dst_y, unsigned int w, unsigned int h)
{
    XCopyArea (ctx->dpy, src->d, ctx->surface->d, ctx->gc,
	       src_x, src_y, w, h, dst_x, dst_y) ;
}

void
_XmPlatBlitMask (XmPlatDrawCtx ctx, XmPlatSurface src, XmPlatSurface mask,
		 int src_x, int src_y, int dst_x, int dst_y,
		 unsigned int w, unsigned int h)
{
    XCopyPlane (ctx->dpy, mask->d, ctx->surface->d, ctx->gc,
		src_x, src_y, w, h, dst_x, dst_y, 1) ;
}

void
_XmPlatClearArea (XmPlatSurface surface, int x, int y,
		  unsigned int w, unsigned int h)
{
    XClearArea (surface->dpy, surface->window, x, y, w, h, False) ;
}

XmPlatFont
_XmPlatFontOfGC (Display *dpy, GC gc)
{
    XGCValues v ;
    XmPlatFont f ;

    XGetGCValues (dpy, gc, GCFont, &v) ;
    f = (XmPlatFont) XtMalloc (sizeof (struct _XmPlatFontRec)) ;
    f->f = NULL ;
    f->fid = v.font ;
    f->kind = XmPlatTextGC ;
    f->dpy = dpy ;
    return f ;
}

/*
 * MB/WC text with a GCFont token needs an XFontSet; the GC carries no
 * font set, so the caller must supply the actual font set via a real
 * token.  This helper is only valid when the caller knows the GC's
 * font is a simple 8/16-bit font (the historic behavior); passing a
 * text kind of MB/WC with a GC token is a call-site error, and we
 * answer it with a fatal-ish empty set rather than crash.  Callers
 * migrated in Phase 2 no longer hit this path.
 */
static XFontSet
_GcFontSetFallback (Display *dpy, GC gc)
{
    static XFontSet empty = NULL ;
    (void) dpy ; (void) gc ;
    if (empty == NULL) {
	char **missing ;
	int n_missing ;
	char *def_string ;
	empty = XCreateFontSet (dpy, "fixed", &missing, &n_missing,
				&def_string) ;
    }
    return empty ;
}

/* --- XftDraw management (Phase 2: moved from XmRenderT.c) ----------- */

/*
 * XftDraw cache.  One XftDraw per (display, window) pair, rebuilt when
 * the visual cannot be used.  Keyed by the surface's drawable; the
 * cache lives as long as the library.
 */
#define XMPLAT_XFT_DRAWCACHE_SIZE 8

typedef struct {
    Display *dpy ;
    Window   window ;
    XftDraw *draw ;
} XmPlatXftDrawCacheEnt ;

static XmPlatXftDrawCacheEnt _XmPlatXftDrawCache[XMPLAT_XFT_DRAWCACHE_SIZE] ;

static XftDraw *
XftDrawForSurface (XmPlatDrawCtx ctx)
{
    Display *dpy = ctx->dpy ;
    Drawable d = ctx->surface->d ;
    Window window = ctx->surface->window ;
    XftDraw *draw ;
    int i ;

    if (window == None) {
	/* Drawable is a pixmap: XftDrawCreateBitmap handles 1-bit
	   pixmaps; deeper pixmaps use the default visual. */
	XWindowAttributes wa ;

	if (XGetWindowAttributes (dpy, d, &wa) == 0) {
	    /* not a window - it is a pixmap */
	    draw = XftDrawCreateBitmap (dpy, d) ;
	    return draw ;
	}
	window = d ;
    }

    for (i = 0; i < XMPLAT_XFT_DRAWCACHE_SIZE; i++) {
	if (_XmPlatXftDrawCache[i].dpy == dpy &&
	    _XmPlatXftDrawCache[i].window == window)
	    return _XmPlatXftDrawCache[i].draw ;
    }

    draw = XftDrawCreate (dpy, window,
			  DefaultVisual (dpy, DefaultScreen (dpy)),
			  DefaultColormap (dpy, DefaultScreen (dpy))) ;
    if (draw == NULL)
	draw = XftDrawCreateBitmap (dpy, window) ;

    /* Store it in the cache. Look for an empty slot first */
    for (i = 0; i < XMPLAT_XFT_DRAWCACHE_SIZE; i++)
	if (_XmPlatXftDrawCache[i].dpy == NULL) {
	    _XmPlatXftDrawCache[i].dpy = dpy ;
	    _XmPlatXftDrawCache[i].window = window ;
	    _XmPlatXftDrawCache[i].draw = draw ;
	    return draw ;
	}
    /* No empty slot - replace a fixed-slot entry (deterministic; the
       cache is only a lookup accelerator, eviction is safe) */
    i = 0 ;
    if (_XmPlatXftDrawCache[i].draw != NULL)
	XftDrawDestroy (_XmPlatXftDrawCache[i].draw) ;
    _XmPlatXftDrawCache[i].dpy = dpy ;
    _XmPlatXftDrawCache[i].window = window ;
    _XmPlatXftDrawCache[i].draw = draw ;
    return draw ;
}

/*
 * Internal seam: hand the cached XftDraw for a surface/ctx pair to
 * legacy Xft code that still manipulates it directly (XmRenderT.c
 * wrappers).  Phase 2 migrates those; the entry point exists so the
 * cache has exactly one owner.
 */
XftDraw *
_XmPlatXftDrawOf (XmPlatDrawCtx ctx, XmPlatSurface surface)
{
    if (ctx->surface == NULL) ctx->surface = surface ;
    return XftDrawForSurface (ctx) ;
}

/* --- text prims (Phase 1 escape hatch) ----------------------------- */

XmPlatFont
_XmPlatFontOfFontStruct (XFontStruct *fs)
{
    XmPlatFont f = (XmPlatFont) XtMalloc (sizeof (struct _XmPlatFontRec)) ;

    f->f = fs ;
    f->fid = 0 ;
    f->kind = XmPlatText8 ;
    f->dpy = NULL ;
    return f ;
}

XmPlatFont
_XmPlatFontOfFontSet (XFontSet fs)
{
    XmPlatFont f = (XmPlatFont) XtMalloc (sizeof (struct _XmPlatFontRec)) ;

    f->f = fs ;
    f->fid = 0 ;
    f->kind = XmPlatTextMB ;
    f->dpy = NULL ;
    return f ;
}

XmPlatFont
_XmPlatFontOfXftFont (void *xftfont)
{
    XmPlatFont f = (XmPlatFont) XtMalloc (sizeof (struct _XmPlatFontRec)) ;

    f->f = xftfont ;
    f->fid = 0 ;
    f->kind = XmPlatTextXFT ;
    f->dpy = NULL ;
    return f ;
}

/* Display-carrying variants: required for Xft tokens (the Xft extents
   calls need a Display); harmless elsewhere. */
static XmPlatFont
FontToken (Display *dpy, void *f, int kind)
{
    XmPlatFont t = (XmPlatFont) XtMalloc (sizeof (struct _XmPlatFontRec)) ;

    t->f = f ;
    t->fid = 0 ;
    t->kind = kind ;
    t->dpy = dpy ;
    return t ;
}

XmPlatFont
_XmPlatFontOfFontStructD (Display *dpy, XFontStruct *fs)
{
    return FontToken (dpy, fs, XmPlatText8) ;
}

XmPlatFont
_XmPlatFontOfFontSetD (Display *dpy, XFontSet fs)
{
    return FontToken (dpy, fs, XmPlatTextMB) ;
}

XmPlatFont
_XmPlatFontOfXftFontD (Display *dpy, void *xftfont)
{
    return FontToken (dpy, xftfont, XmPlatTextXFT) ;
}

/* Display of a font token (NULL if unknown). */
Display *
_XmPlatFontDisplay (XmPlatFont font)
{
    return font->dpy ;
}

void *
_XmPlatFontBacking (XmPlatFont font)
{
    return font->f ;
}

/* ---- font metrics (Phase 2 contract) ------------------------------- */

/*
 * A FontSet token answers line metrics from its first font
 * (XFontsOfFontSet()[0]); this matches what the callers in
 * XmFontList.c/_XmGetFirstFont did before migration.
 */

static XFontStruct * FontSetFirstFont (XFontSet fset) ;

XFontStruct *
_XmPlatFontSetFirstStruct (XmPlatFont font)
{
    if (font->kind != XmPlatTextMB) return NULL ;
    return FontSetFirstFont ((XFontSet) font->f) ;
}

static XFontStruct *
FontSetFirstFont (XFontSet fset)
{
    XFontStruct **fs_list ;
    char **name_list ;

    if (XFontsOfFontSet (fset, &fs_list, &name_list) > 0)
	return fs_list[0] ;
    return NULL ;
}

int
_XmPlatFontKind (XmPlatFont font)
{
    return font->kind ;
}

void
_XmPlatFontFree (XmPlatFont font)
{
    if (font) XtFree ((char *) font) ;
}

XmPlatFont
_XmPlatFontLoad (Display *dpy, const char *name)
{
    XFontStruct *fs = XLoadQueryFont (dpy, name) ;

    if (fs == NULL) return NULL ;
    return FontToken (dpy, fs, XmPlatText8) ;
}

void
_XmPlatFontUnload (XmPlatFont font)
{
    if (font == NULL) return ;
    if (font->kind == XmPlatText8 && font->f != NULL)
	XFreeFont (font->dpy, (XFontStruct *) font->f) ;
    _XmPlatFontFree (font) ;
}

int
_XmPlatFontAscent (XmPlatFont font)
{
    switch (font->kind) {
    case XmPlatTextXFT:
	return ((XftFont *) font->f)->ascent ;
    case XmPlatTextMB: {
	XFontStruct *fs = FontSetFirstFont ((XFontSet) font->f) ;
	return fs ? fs->ascent : 0 ;
    }
    case XmPlatText8:
    case XmPlatText16:
	return ((XFontStruct *) font->f)->ascent ;
    default:
	return 0 ;
    }
}

int
_XmPlatFontDescent (XmPlatFont font)
{
    switch (font->kind) {
    case XmPlatTextXFT:
	return ((XftFont *) font->f)->descent ;
    case XmPlatTextMB: {
	XFontStruct *fs = FontSetFirstFont ((XFontSet) font->f) ;
	return fs ? fs->descent : 0 ;
    }
    case XmPlatText8:
    case XmPlatText16:
	return ((XFontStruct *) font->f)->descent ;
    default:
	return 0 ;
    }
}

int
_XmPlatFontHeight (XmPlatFont font)
{
    switch (font->kind) {
    case XmPlatTextXFT:
	return ((XftFont *) font->f)->height ;
    case XmPlatTextMB: {
	XFontStruct *fs = FontSetFirstFont ((XFontSet) font->f) ;
	return fs ? (fs->ascent + fs->descent) : 0 ;
    }
    case XmPlatText8:
    case XmPlatText16: {
	XFontStruct *fs = (XFontStruct *) font->f ;
	return fs->ascent + fs->descent ;
    }
    default:
	return 0 ;
    }
}

int
_XmPlatFontAverageWidth (XmPlatFont font)
{
    switch (font->kind) {
    case XmPlatTextXFT:
	return ((XftFont *) font->f)->max_advance_width ;
    case XmPlatTextMB: {
	XFontStruct *fs = FontSetFirstFont ((XFontSet) font->f) ;
	return fs ? (fs->min_bounds.width + fs->max_bounds.width) / 2 : 0 ;
    }
    case XmPlatText8:
    case XmPlatText16: {
	XFontStruct *fs = (XFontStruct *) font->f ;
	return (fs->min_bounds.width + fs->max_bounds.width) / 2 ;
    }
    default:
	return 0 ;
    }
}

int
_XmPlatDigitWidth (XmPlatFont font, int n)
{
    static const char digit = '0' ;

    if (n <= 0) return 0 ;
    return n * _XmPlatTextWidth (font, XmPlatText8, &digit, 1) ;
}

void
_XmPlatDrawString (XmPlatDrawCtx ctx, XmPlatFont font, int kind,
		   const void *text, int len, int x, int y, int image)
{
    _XmPlatDrawStringColored (ctx, font, kind, text, len, x, y, image,
			      NULL) ;
}

void
_XmPlatDrawStringColored (XmPlatDrawCtx ctx, XmPlatFont font, int kind,
			  const void *text, int len,
			  int x, int y, int image,
			  const XmPlatColor *color)
{
    Drawable d = ctx->surface->d ;
    GC gc = ctx->gc ;

    /* Explicit-color path: push the pixel into the GC (core-X backend
       keeps color == pixel; alpha-capable backends read the fields). */
    if (color != NULL) {
	XGCValues v ;
	unsigned long mask = GCForeground ;

	if (color->pixel == 0 &&
	    (color->red | color->green | color->blue) != 0) {
	    XColor xcol ;

	    xcol.red = color->red ; xcol.green = color->green ;
	    xcol.blue = color->blue ;
	    if (XAllocColor (ctx->dpy,
			     DefaultColormap (ctx->dpy,
					      DefaultScreen (ctx->dpy)),
			     &xcol)) {
		v.foreground = xcol.pixel ;
	    } else {
		v.foreground = BlackPixel (ctx->dpy,
					   DefaultScreen (ctx->dpy)) ;
	    }
	} else {
	    v.foreground = color->pixel ;
	}
	XChangeGC (ctx->dpy, gc, mask, &v) ;
    }

    if (font->kind == XmPlatTextXFT) {
	XftDraw *draw = XftDrawForSurface (ctx) ;
	XGCValues gv ;
	XColor xcol ;
	XftColor xftcol ;

	XGetGCValues (ctx->dpy, gc, GCForeground, &gv) ;
	xcol.pixel = gv.foreground ;
	XQueryColor (ctx->dpy,
		     DefaultColormap (ctx->dpy, DefaultScreen (ctx->dpy)),
		     &xcol) ;
	xftcol.pixel = xcol.pixel ;
	xftcol.color.red = xcol.red ;
	xftcol.color.green = xcol.green ;
	xftcol.color.blue = xcol.blue ;
	xftcol.color.alpha = 0xFFFF ;

	if (kind == XmPlatText16) {
	    if (image)
		XftDrawString16 (draw, &xftcol, (XftFont *) font->f,
				 x, y, (const XftChar16 *) text, len) ;
	    else
		XftDrawString16 (draw, &xftcol, (XftFont *) font->f,
				 x, y, (const XftChar16 *) text, len) ;
	} else {
	    if (image)
		XftDrawStringUtf8 (draw, &xftcol, (XftFont *) font->f,
				   x, y, (const XftChar8 *) text, len) ;
	    else
		XftDrawStringUtf8 (draw, &xftcol, (XftFont *) font->f,
				   x, y, (const XftChar8 *) text, len) ;
	}
	return ;
    }

    if (font->kind == XmPlatTextGC) {
        /* font rides in the GC; kind selects 8/16/mb by the call site */
        if (kind == XmPlatText16) {
            if (image)
                XDrawImageString16 (ctx->dpy, d, gc,
                                    x, y, (XChar2b *) text, len) ;
            else
                XDrawString16 (ctx->dpy, d, gc,
                               x, y, (XChar2b *) text, len) ;
        } else if (kind == XmPlatTextMB) {
            if (image)
                XmbDrawImageString (ctx->dpy, d,
				    _GcFontSetFallback (ctx->dpy, gc),
				    gc, x, y, (const char *) text, len) ;
            else
                XmbDrawString (ctx->dpy, d,
			       _GcFontSetFallback (ctx->dpy, gc),
			       gc, x, y, (const char *) text, len) ;
        } else {
            if (image)
                XDrawImageString (ctx->dpy, d, gc, x, y,
                                  (const char *) text, len) ;
            else
                XDrawString (ctx->dpy, d, gc, x, y,
                             (const char *) text, len) ;
        }
        return ;
    }

    if (font->kind == XmPlatText8 || font->kind == XmPlatText16) {
	XFontStruct *fs = (XFontStruct *) font->f ;
	if (kind == XmPlatText16) {
	    if (image)
		XDrawImageString16 (ctx->dpy, d, gc,
				    x, y, (XChar2b *) text, len) ;
	    else
		XDrawString16 (ctx->dpy, d, gc,
			       x, y, (XChar2b *) text, len) ;
	} else {
	    if (image)
		XDrawImageString (ctx->dpy, d, gc, x, y,
				  (const char *) text, len) ;
	    else
		XDrawString (ctx->dpy, d, gc, x, y,
			     (const char *) text, len) ;
	}
	(void) fs ;
    } else {
	XFontSet fset = (XFontSet) font->f ;
	if (kind == XmPlatTextWC) {
	    if (image)
		XwcDrawImageString (ctx->dpy, d, fset, gc, x, y,
				    (wchar_t *) text, len) ;
	    else
		XwcDrawString (ctx->dpy, d, fset, gc, x, y,
			       (wchar_t *) text, len) ;
	} else if (kind == XmPlatTextUTF8) {
	    if (image)
		Xutf8DrawImageString (ctx->dpy, d, fset, gc, x, y,
				      (const char *) text, len) ;
	    else
		Xutf8DrawString (ctx->dpy, d, fset, gc, x, y,
				 (const char *) text, len) ;
	} else {
	    if (image)
		XmbDrawImageString (ctx->dpy, d, fset, gc, x, y,
				    (const char *) text, len) ;
	    else
		XmbDrawString (ctx->dpy, d, fset, gc, x, y,
			       (const char *) text, len) ;
	}
    }
}

/* ---- string metrics (Phase 2 contract) ----------------------------- */

/*
 * Advance width for the XFontStruct path: per-char table lookup with
 * default-char / min-bounds fallbacks, exactly what the pre-contract
 * TextOut/TextF/DatF helpers did inline.
 */
static int
FontStructTextWidth (XFontStruct *fs, int kind, const void *text, int len)
{
    if (kind == XmPlatText16) {
	const XChar2b *s = (const XChar2b *) text ;
	int i, w = 0 ;

	for (i = 0; i < len; i++, s++) {
	    unsigned int c = ((unsigned int) s->byte1 << 8) | s->byte2 ;
	    if (fs->per_char &&
		c >= (unsigned int) fs->min_char_or_byte2 &&
		c <= (unsigned int) fs->max_char_or_byte2)
		w += fs->per_char[c - fs->min_char_or_byte2].width ;
	    else if (fs->per_char &&
		     fs->default_char >= fs->min_char_or_byte2 &&
		     fs->default_char <= fs->max_char_or_byte2)
		w += fs->per_char[fs->default_char -
				  fs->min_char_or_byte2].width ;
	    else
		w += fs->min_bounds.width ;
	}
	return w ;
    } else {
	return XTextWidth (fs, (const char *) text, len) ;
    }
}

static void
FontStructTextExtents (XFontStruct *fs, int kind, const void *text, int len,
		       XmPlatCharInfo *overall)
{
    int dir, fascent, fdescent ;
    XCharStruct cs ;

    if (kind == XmPlatText16)
	XTextExtents16 (fs, (XChar2b *) text, len,
			&dir, &fascent, &fdescent, &cs) ;
    else
	XTextExtents (fs, (const char *) text, len,
		      &dir, &fascent, &fdescent, &cs) ;
    overall->lbearing = cs.lbearing ;
    overall->rbearing = cs.rbearing ;
    overall->width = cs.width ;
    overall->ascent = cs.ascent ;
    overall->descent = cs.descent ;
    overall->attributes = cs.attributes ;
}

int
_XmPlatTextWidth (XmPlatFont font, int kind, const void *text, int len)
{
    switch (font->kind) {
    case XmPlatTextXFT: {
	XGlyphInfo ext ;

	switch (kind) {
	case XmPlatText16:
	    XftTextExtents16 (font->dpy, (XftFont *) font->f,
			      (const XftChar16 *) text, len, &ext) ;
	    break ;
	default: /* 8-bit, MB treated as 8-bit for Xft */
	    XftTextExtents8 (font->dpy, (XftFont *) font->f,
			     (const XftChar8 *) text, len, &ext) ;
	    break ;
	}
	return ext.xOff ;
    }
    case XmPlatTextMB: {
	/* MB string: XmbTextEscapement handles charset resolution */
	return XmbTextEscapement ((XFontSet) font->f,
				  (const char *) text, len) ;
    }
    case XmPlatTextWC:
	return XwcTextEscapement ((XFontSet) font->f,
				  (const wchar_t *) text, len) ;
    case XmPlatText8:
    case XmPlatText16:
	return FontStructTextWidth ((XFontStruct *) font->f, kind, text, len) ;
    default:
	return 0 ;
    }
}

void
_XmPlatTextExtents (XmPlatFont font, int kind, const void *text, int len,
		    XmPlatCharInfo *overall)
{
    switch (font->kind) {
    case XmPlatTextXFT: {
	XGlyphInfo ext ;

	switch (kind) {
	case XmPlatText16:
	    XftTextExtents16 (font->dpy, (XftFont *) font->f,
			      (const XftChar16 *) text, len, &ext) ;
	    break ;
	default:
	    XftTextExtents8 (font->dpy, (XftFont *) font->f,
			     (const XftChar8 *) text, len, &ext) ;
	    break ;
	}
	overall->lbearing = (short) ext.x ;
	overall->rbearing = (short) (ext.x + ext.width) ;
	overall->width = (short) ext.xOff ;
	/* XGlyphInfo geometry: origin is the glyph center; ascent/descent
	   in the core-X sense map as height - y and y. */
	overall->ascent = (short) (ext.height - ext.y) ;
	overall->descent = (short) ext.y ;
	overall->attributes = 0 ;
	break ;
    }
    case XmPlatTextMB: {
	XRectangle ink, logical ;

	XmbTextExtents ((XFontSet) font->f, (const char *) text, len,
			&ink, &logical) ;
	overall->lbearing = (short) ink.x ;
	overall->rbearing = (short) (ink.x + ink.width) ;
	overall->width = (short) logical.width ;
	overall->ascent = (short) (logical.height + logical.y) ;
	overall->descent = (short) -(logical.y) ;
	overall->attributes = 0 ;
	break ;
    }
    case XmPlatTextWC: {
	XRectangle ink, logical ;

	XwcTextExtents ((XFontSet) font->f, (const wchar_t *) text, len,
			&ink, &logical) ;
	overall->lbearing = (short) ink.x ;
	overall->rbearing = (short) (ink.x + ink.width) ;
	overall->width = (short) logical.width ;
	overall->ascent = (short) (logical.height + logical.y) ;
	overall->descent = (short) -(logical.y) ;
	overall->attributes = 0 ;
	break ;
    }
    case XmPlatText8:
    case XmPlatText16:
	FontStructTextExtents ((XFontStruct *) font->f, kind, text, len,
			       overall) ;
	break ;
    default:
	overall->lbearing = overall->rbearing = overall->width = 0 ;
	overall->ascent = overall->descent = overall->attributes = 0 ;
	break ;
    }
}

/* ---- image contract (Phase 3) --------------------------------------- */

/*
 * The X11 backend stores the XImage pointer directly in the token (the
 * Phase-1 seam cast did the same); struct _XmPlatImageRec lives here.
 */
XmPlatImage
_XmPlatImageCreate (XmPlatDrawCtx ctx, int depth,
		    unsigned int width, unsigned int height)
{
    Display *dpy = ctx->dpy ;
    XImage *xi ;
    XmPlatImage img ;

    xi = XCreateImage (dpy,
		       DefaultVisual (dpy, DefaultScreen (dpy)),
		       depth, (depth == 1) ? XYBitmap : ZPixmap,
		       0, NULL, width, height, 8, 0) ;
    if (xi == NULL) return NULL ;
    /* XCreateImage left data NULL; allocate it ourselves. */
    xi->data = XtMalloc (xi->bytes_per_line * (int) height) ;
    memset (xi->data, 0, (size_t) (xi->bytes_per_line * (int) height)) ;

    img = (XmPlatImage) XtMalloc (sizeof (struct _XmPlatImageRec)) ;
    img->ximage = xi ;
    return img ;
}

XmPlatImage
_XmPlatImageCreateOnVisual (XmPlatDrawCtx ctx, const void *visual,
			    int depth, unsigned int width,
			    unsigned int height)
{
    Display *dpy = ctx->dpy ;
    XImage *xi ;
    XmPlatImage img ;

    xi = XCreateImage (dpy, (Visual *) visual,
		       depth, (depth == 1) ? XYBitmap : ZPixmap,
		       0, NULL, width, height, 8, 0) ;
    if (xi == NULL) return NULL ;
    xi->data = XtMalloc (xi->bytes_per_line * (int) height) ;
    memset (xi->data, 0, (size_t) (xi->bytes_per_line * (int) height)) ;

    img = (XmPlatImage) XtMalloc (sizeof (struct _XmPlatImageRec)) ;
    img->ximage = xi ;
    return img ;
}

XmPlatImage
_XmPlatImageCreateBitmap (void *display, char *data,
			  unsigned int width, unsigned int height)
{
    Display *dpy = (Display *) display ;
    XImage *xi ;
    XmPlatImage img ;

    /* the seam always supplies the display; no default-display fallback
       exists inside XmPlat (Xt stays at the caller side) */
    xi = XCreateImage (dpy,
		       DefaultVisual (dpy, DefaultScreen (dpy)),
		       1, XYBitmap, 0, data, width, height,
		       8, (int) ((width + 7) >> 3)) ;
    if (xi == NULL) return NULL ;
    /* match the _XmCreateImage macro's client-specific fields (BUG 4262) */
    xi->byte_order = LSBFirst ;
    xi->bitmap_unit = 8 ;
    xi->bitmap_bit_order = LSBFirst ;

    img = (XmPlatImage) XtMalloc (sizeof (struct _XmPlatImageRec)) ;
    img->ximage = xi ;
    return img ;
}

void
_XmPlatImageFree (XmPlatImage image)
{
    if (image == NULL) return ;
    if (image->ximage != NULL)
	XDestroyImage (image->ximage) ;
    XtFree ((char *) image) ;
}

unsigned int
_XmPlatImageWidth (XmPlatImage image)
{
    return (unsigned int) image->ximage->width ;
}

unsigned int
_XmPlatImageHeight (XmPlatImage image)
{
    return (unsigned int) image->ximage->height ;
}

int
_XmPlatImageDepth (XmPlatImage image)
{
    return image->ximage->depth ;
}

char *
_XmPlatImageData (XmPlatImage image)
{
    return image->ximage->data ;
}

int
_XmPlatImageBytesPerLine (XmPlatImage image)
{
    return image->ximage->bytes_per_line ;
}

unsigned long
_XmPlatImageGetPixel (XmPlatImage image, int x, int y)
{
    return XGetPixel (image->ximage, x, y) ;
}

void
_XmPlatImagePutPixel (XmPlatImage image, int x, int y, unsigned long pixel)
{
    XPutPixel (image->ximage, x, y, pixel) ;
}

XmPlatImage
_XmPlatImageSub (XmPlatImage image, int x, int y,
		 unsigned int w, unsigned int h)
{
    XImage *xi = XSubImage (image->ximage, x, y, (unsigned) w, (unsigned) h) ;
    XmPlatImage img ;

    if (xi == NULL) return NULL ;
    img = (XmPlatImage) XtMalloc (sizeof (struct _XmPlatImageRec)) ;
    img->ximage = xi ;
    return img ;
}

XmPlatImage
_XmPlatImageFromSurface (XmPlatDrawCtx ctx, XmPlatSurface src,
			 int src_x, int src_y,
			 unsigned int w, unsigned int h)
{
    XImage *xi ;
    XmPlatImage img ;

    xi = XGetImage (ctx->dpy, src->d, src_x, src_y,
		    (int) w, (int) h, AllPlanes, ZPixmap) ;
    if (xi == NULL) return NULL ;
    img = (XmPlatImage) XtMalloc (sizeof (struct _XmPlatImageRec)) ;
    img->ximage = xi ;
    return img ;
}

/* Phase-1 seam inverse (XmPlatP.h declares; transitional). */
XImage *
_XmPlatImageXImage (XmPlatImage image)
{
    return image->ximage ;
}

/* Token builder from an existing XImage (frozen-API boundary use). */
XmPlatImage
_XmPlatImageTokenOf (XImage *ximage)
{
    XmPlatImage img ;

    if (ximage == NULL) return NULL ;
    img = (XmPlatImage) XtMalloc (sizeof (struct _XmPlatImageRec)) ;
    img->ximage = ximage ;
    return img ;
}

void
_XmPlatImageTokenFree (XmPlatImage image)
{
    if (image) XtFree ((char *) image) ;
}

/* Typed bitmap creator (seam). */
XmPlatImage
_XmPlatImageBitmapOf (Display *dpy, char *data,
		      unsigned int width, unsigned int height)
{
    return _XmPlatImageCreateBitmap ((void *) dpy, data, width, height) ;
}

XmPlatImage
_XmPlatImageFromSurface2 (Display *dpy, Drawable d, int src_x, int src_y,
			  unsigned int w, unsigned int h)
{
    XImage *xi ;
    XmPlatImage img ;

    xi = XGetImage (dpy, d, src_x, src_y, (int) w, (int) h,
		    AllPlanes, ZPixmap) ;
    if (xi == NULL) return NULL ;
    img = (XmPlatImage) XtMalloc (sizeof (struct _XmPlatImageRec)) ;
    img->ximage = xi ;
    return img ;
}

XImage *
_XmPlatImageRawCreate (Display *dpy, Visual *visual, int depth, int format,
		       unsigned int w, unsigned int h, int bitmap_pad)
{
    return XCreateImage (dpy, visual, depth, format, 0, NULL,
			 (unsigned) w, (unsigned) h, bitmap_pad, 0) ;
}

/* ---- Event contract (Phase 4) ------------------------------------------ */

/*
 * The X11 event token wraps the XEvent record without copying it.
 * Handler code reads fields through the prims; the raw pointer stays
 * reachable for frozen Xt signatures (_XmPlatEventRaw).  Copy/synth
 * tokens own a heap XEvent and free it in _XmPlatEventFreeCopy.
 */
struct _XmPlatEventRec {
    XEvent *ev ;		/* NULL for None tokens */
    Boolean owned ;		/* XtFree the record in FreeCopy */
} ;

static struct _XmPlatEventRec _XmPlatEventNoneRec = { NULL, False } ;

XmPlatEvent
_XmPlatEventOf (const void *raw_event)
{
    XmPlatEvent pev ;

    if (raw_event == NULL)
	return (XmPlatEvent) &_XmPlatEventNoneRec ;
    pev = (XmPlatEvent) XtMalloc (sizeof (struct _XmPlatEventRec)) ;
    pev->ev = (XEvent *) raw_event ;
    pev->owned = False ;
    return pev ;
}

XmPlatEvent
_XmPlatEventCopy (XmPlatEvent ev)
{
    XmPlatEvent copy ;

    if (ev == NULL || ev->ev == NULL)
	return (XmPlatEvent) &_XmPlatEventNoneRec ;
    copy = (XmPlatEvent) XtMalloc (sizeof (struct _XmPlatEventRec)) ;
    copy->ev = (XEvent *) XtMalloc (sizeof (XEvent)) ;
    memcpy ((void *) copy->ev, (void *) ev->ev, sizeof (XEvent)) ;
    copy->owned = True ;
    return copy ;
}

void
_XmPlatEventFreeCopy (XmPlatEvent ev)
{
    if (ev == NULL || ev == (XmPlatEvent) &_XmPlatEventNoneRec) return ;
    if (ev->owned) XtFree ((char *) (void *) ev->ev) ;
    XtFree ((char *) ev) ;
}

XmPlatEvent
_XmPlatEventSynth (XmPlatEvent ev, XmPlatEventKind kind)
{
    XmPlatEvent synth ;
    XEvent *se ;
    int x11type ;

    if (ev == NULL || ev->ev == NULL)
	return (XmPlatEvent) &_XmPlatEventNoneRec ;
    synth = _XmPlatEventCopy (ev) ;
    se = (XEvent *) synth->ev ;
    switch (kind) {
    case XmPlatEventPointer:
	/* Keep press/release vs motion split; default to motion. */
	if (se->type != ButtonPress && se->type != ButtonRelease)
	    x11type = MotionNotify ;
	else
	    x11type = se->type ;
	break ;
    case XmPlatButtonPress:	x11type = ButtonPress ;	 break ;
    case XmPlatButtonRelease:	x11type = ButtonRelease ; break ;
    case XmPlatEventKey:	x11type = KeyPress ;	 break ;
    case XmPlatEventCrossing:	x11type = EnterNotify ;	 break ;
    case XmPlatEventFocus:	x11type = FocusIn ;	 break ;
    case XmPlatEventExpose:	x11type = Expose ;	 break ;
    case XmPlatEventConfigure:	x11type = ConfigureNotify ; break ;
    case XmPlatEventMap:	x11type = MapNotify ;	 break ;
    case XmPlatEventUnmap:	x11type = UnmapNotify ;	 break ;
    default:			x11type = se->type ;	 break ;
    }
    se->type = x11type ;
    return synth ;
}

void *
_XmPlatEventRaw (XmPlatEvent ev)
{
    if (ev == NULL || ev->ev == NULL) return NULL ;
    return (void *) ev->ev ;
}

#define EV (ev->ev)

XmPlatEventKind
_XmPlatEventKind (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return XmPlatEventNone ;
    switch (EV->type) {
    case ButtonPress:
    case ButtonRelease:
    case MotionNotify:		return XmPlatEventPointer ;
    case KeyPress:
    case KeyRelease:		return XmPlatEventKey ;
    case EnterNotify:
    case LeaveNotify:		return XmPlatEventCrossing ;
    case FocusIn:
    case FocusOut:		return XmPlatEventFocus ;
    case Expose:		return XmPlatEventExpose ;
    case GraphicsExpose:	return XmPlatEventGraphicsExpose ;
    case NoExpose:		return XmPlatEventNoExpose ;
    case ConfigureNotify:	return XmPlatEventConfigure ;
    case MapNotify:		return XmPlatEventMap ;
    case UnmapNotify:		return XmPlatEventUnmap ;
    case VisibilityNotify:	return XmPlatEventVisibility ;
    case ReparentNotify:	return XmPlatEventReparent ;
    case PropertyNotify:	return XmPlatEventProperty ;
    case ClientMessage:		return XmPlatEventClientMessage ;
    case SelectionNotify:	return XmPlatEventSelection ;
    case SelectionRequest:	return XmPlatEventSelectionRequest ;
    case SelectionClear:	return XmPlatEventSelectionClear ;
    case ColormapNotify:	return XmPlatEventColormap ;
    case CirculateNotify:	return XmPlatEventCirculate ;
    default:			return XmPlatEventOther ;
    }
}

XmPlatBoolean
_XmPlatEventIsType (XmPlatEvent ev, XmPlatEventKind kind)
{
    return _XmPlatEventKind (ev) == kind ;
}

unsigned long
_XmPlatEventSerial (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    return EV->xany.serial ;
}

XmPlatBoolean
_XmPlatEventSendEvent (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return False ;
    return (XmPlatBoolean) EV->xany.send_event ;
}

XmPlatTime
_XmPlatEventTime (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    switch (EV->type) {
    case ButtonPress:
    case ButtonRelease:		return EV->xbutton.time ;
    case KeyPress:
    case KeyRelease:		return EV->xkey.time ;
    case MotionNotify:		return EV->xmotion.time ;
    case EnterNotify:
    case LeaveNotify:		return EV->xcrossing.time ;
    case PropertyNotify:	return EV->xproperty.time ;
    case SelectionNotify:	return EV->xselection.time ;
    case SelectionRequest:	return EV->xselectionrequest.time ;
    case SelectionClear:	return EV->xselectionclear.time ;
    default:			return 0 ;
    }
}

XmPlatWindow
_XmPlatEventWindow (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return XmPlatWindowNone ;
    return (XmPlatWindow) EV->xany.window ;
}

int
_XmPlatEventX (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    switch (EV->type) {
    case ButtonPress:
    case ButtonRelease:		return EV->xbutton.x ;
    case MotionNotify:		return EV->xmotion.x ;
    case EnterNotify:
    case LeaveNotify:		return EV->xcrossing.x ;
    case KeyPress:
    case KeyRelease:		return EV->xkey.x ;
    case Expose:		return EV->xexpose.x ;
    case ConfigureNotify:	return EV->xconfigure.x ;
    case ReparentNotify:	return EV->xreparent.x ;
    default:			return 0 ;
    }
}

int
_XmPlatEventY (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    switch (EV->type) {
    case ButtonPress:
    case ButtonRelease:		return EV->xbutton.y ;
    case MotionNotify:		return EV->xmotion.y ;
    case EnterNotify:
    case LeaveNotify:		return EV->xcrossing.y ;
    case KeyPress:
    case KeyRelease:		return EV->xkey.y ;
    case Expose:		return EV->xexpose.y ;
    case ConfigureNotify:	return EV->xconfigure.y ;
    case ReparentNotify:	return EV->xreparent.y ;
    default:			return 0 ;
    }
}

int
_XmPlatEventRootX (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    switch (EV->type) {
    case ButtonPress:
    case ButtonRelease:		return EV->xbutton.x_root ;
    case MotionNotify:		return EV->xmotion.x_root ;
    case EnterNotify:
    case LeaveNotify:		return EV->xcrossing.x_root ;
    case KeyPress:
    case KeyRelease:		return EV->xkey.x_root ;
    default:			return 0 ;
    }
}

int
_XmPlatEventRootY (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    switch (EV->type) {
    case ButtonPress:
    case ButtonRelease:		return EV->xbutton.y_root ;
    case MotionNotify:		return EV->xmotion.y_root ;
    case EnterNotify:
    case LeaveNotify:		return EV->xcrossing.y_root ;
    case KeyPress:
    case KeyRelease:		return EV->xkey.y_root ;
    default:			return 0 ;
    }
}

unsigned int
_XmPlatEventButton (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    switch (EV->type) {
    case ButtonPress:
    case ButtonRelease:		return EV->xbutton.button ;
    default:			return 0 ;
    }
}

unsigned int
_XmPlatEventState (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    switch (EV->type) {
    case ButtonPress:
    case ButtonRelease:		return EV->xbutton.state ;
    case MotionNotify:		return EV->xmotion.state ;
    case KeyPress:
    case KeyRelease:		return EV->xkey.state ;
    case EnterNotify:
    case LeaveNotify:		return EV->xcrossing.state ;
    default:			return 0 ;
    }
}

XmPlatBoolean
_XmPlatEventIsHint (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return False ;
    if (EV->type == MotionNotify) return (XmPlatBoolean) EV->xmotion.is_hint ;
    return False ;
}

int
_XmPlatEventPointerKind (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    switch (EV->type) {
    case ButtonPress:		return XmPlatButtonPress ;
    case ButtonRelease:		return XmPlatButtonRelease ;
    case MotionNotify:		return XmPlatPointerMotion ;
    default:			return 0 ;
    }
}

void
_XmPlatEventSetX (XmPlatEvent ev, int x)
{
    if (ev == NULL || ev->ev == NULL) return ;
    switch (ev->ev->type) {
    case ButtonPress:
    case ButtonRelease:	ev->ev->xbutton.x = x ;	break ;
    case MotionNotify:	ev->ev->xmotion.x = x ;	break ;
    case EnterNotify:
    case LeaveNotify:	ev->ev->xcrossing.x = x ; break ;
    case KeyPress:
    case KeyRelease:	ev->ev->xkey.x = x ;	break ;
    case Expose:	ev->ev->xexpose.x = x ;	break ;
    default:					break ;
    }
}

void
_XmPlatEventSetY (XmPlatEvent ev, int y)
{
    if (ev == NULL || ev->ev == NULL) return ;
    switch (ev->ev->type) {
    case ButtonPress:
    case ButtonRelease:	ev->ev->xbutton.y = y ;	break ;
    case MotionNotify:	ev->ev->xmotion.y = y ;	break ;
    case EnterNotify:
    case LeaveNotify:	ev->ev->xcrossing.y = y ; break ;
    case KeyPress:
    case KeyRelease:	ev->ev->xkey.y = y ;	break ;
    case Expose:	ev->ev->xexpose.y = y ;	break ;
    default:					break ;
    }
}

unsigned int
_XmPlatEventKeycode (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    switch (EV->type) {
    case KeyPress:
    case KeyRelease:		return EV->xkey.keycode ;
    default:			return 0 ;
    }
}

unsigned long
_XmPlatEventKeysym (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return NoSymbol ;
    switch (EV->type) {
    case KeyPress:
    case KeyRelease:
	return XLookupKeysym ((XKeyEvent *) (void *) EV, 0) ;
    default:
	return NoSymbol ;
    }
}

XmPlatBoolean
_XmPlatEventFocus (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return False ;
    if (EV->type == EnterNotify || EV->type == LeaveNotify)
	return (XmPlatBoolean) EV->xcrossing.focus ;
    return False ;
}

int
_XmPlatEventMode (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return XmPlatNotifyNormal ;
    switch (EV->type) {
    case EnterNotify:
    case LeaveNotify:		return EV->xcrossing.mode ;
    case FocusIn:
    case FocusOut:		return EV->xfocus.mode ;
    default:			return XmPlatNotifyNormal ;
    }
}

int
_XmPlatEventDetail (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    switch (EV->type) {
    case EnterNotify:
    case LeaveNotify:		return EV->xcrossing.detail ;
    case FocusIn:
    case FocusOut:		return EV->xfocus.detail ;
    default:			return 0 ;
    }
}

XmPlatWindow
_XmPlatEventSubwindow (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return XmPlatWindowNone ;
    switch (EV->type) {
    case EnterNotify:
    case LeaveNotify:		return (XmPlatWindow) EV->xcrossing.subwindow ;
    case MotionNotify:		return (XmPlatWindow) EV->xmotion.subwindow ;
    default:			return XmPlatWindowNone ;
    }
}

XmPlatBoolean
_XmPlatEventIsKeyPress (XmPlatEvent ev)
{
    if (ev == NULL || ev->ev == NULL) return False ;
    return (XmPlatBoolean) (ev->ev->type == KeyPress) ;
}

XmPlatBoolean
_XmPlatEventIsKeyRelease (XmPlatEvent ev)
{
    if (ev == NULL || ev->ev == NULL) return False ;
    return (XmPlatBoolean) (ev->ev->type == KeyRelease) ;
}

XmPlatBoolean
_XmPlatEventIsEnter (XmPlatEvent ev)
{
    if (ev == NULL || ev->ev == NULL) return False ;
    return (XmPlatBoolean) (ev->ev->type == EnterNotify) ;
}

XmPlatBoolean
_XmPlatEventIsLeave (XmPlatEvent ev)
{
    if (ev == NULL || ev->ev == NULL) return False ;
    return (XmPlatBoolean) (ev->ev->type == LeaveNotify) ;
}

XmPlatBoolean
_XmPlatEventIsFocusIn (XmPlatEvent ev)
{
    if (ev == NULL || ev->ev == NULL) return False ;
    return (XmPlatBoolean) (ev->ev->type == FocusIn) ;
}

XmPlatBoolean
_XmPlatEventIsFocusOut (XmPlatEvent ev)
{
    if (ev == NULL || ev->ev == NULL) return False ;
    return (XmPlatBoolean) (ev->ev->type == FocusOut) ;
}

unsigned int
_XmPlatEventWidth (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    switch (EV->type) {
    case Expose:		return EV->xexpose.width ;
    case GraphicsExpose:	return EV->xgraphicsexpose.width ;
    case ConfigureNotify:	return EV->xconfigure.width ;
    case ResizeRequest:		return EV->xresizerequest.width ;
    default:			return 0 ;
    }
}

unsigned int
_XmPlatEventHeight (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    switch (EV->type) {
    case Expose:		return EV->xexpose.height ;
    case GraphicsExpose:	return EV->xgraphicsexpose.height ;
    case ConfigureNotify:	return EV->xconfigure.height ;
    case ResizeRequest:		return EV->xresizerequest.height ;
    default:			return 0 ;
    }
}

unsigned int
_XmPlatEventCount (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    if (EV->type == Expose) return EV->xexpose.count ;
    if (EV->type == GraphicsExpose) return EV->xgraphicsexpose.count ;
    return 0 ;
}

unsigned int
_XmPlatEventBorderWidth (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return 0 ;
    switch (EV->type) {
    case ConfigureNotify:	return EV->xconfigure.border_width ;
    case CreateNotify:		return EV->xcreatewindow.border_width ;
    default:			return 0 ;
    }
}

int
_XmPlatEventVisibilityState (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return XmPlatVisibilityUnobscured ;
    if (EV->type == VisibilityNotify) return EV->xvisibility.state ;
    return XmPlatVisibilityUnobscured ;
}

const char *
_XmPlatEventPropertyName (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return NULL ;
    switch (EV->type) {
    case PropertyNotify:
	return XGetAtomName (EV->xproperty.display, EV->xproperty.atom) ;
    case SelectionRequest:
	return XGetAtomName (EV->xselectionrequest.display,
			     EV->xselectionrequest.property) ;
    case SelectionNotify:
	return XGetAtomName (EV->xselection.display,
			     EV->xselection.property) ;
    case SelectionClear:
	return XGetAtomName (EV->xselectionclear.display,
			     EV->xselectionclear.selection) ;
    default:
	return NULL ;
    }
}

XmPlatWindow
_XmPlatEventRequestor (XmPlatEvent ev)
{
    if (ev == NULL || EV == NULL) return XmPlatWindowNone ;
    if (EV->type == SelectionRequest)
	return (XmPlatWindow) EV->xselectionrequest.requestor ;
    return XmPlatWindowNone ;
}

#undef EV

Display *
_XmPlatEventDisplayOf (XmPlatEvent ev)
{
    if (ev == NULL || ev->ev == NULL) return NULL ;
    return ev->ev->xany.display ;
}

unsigned long
_XmPlatInternAtomRaw (Display *dpy, const char *name,
		      XmPlatBoolean only_if_exists)
{
    if (dpy == NULL || name == NULL) return 0 ;
    return (unsigned long) XInternAtom (dpy, name,
					only_if_exists ? True : False) ;
}

void
_XmPlatInternAtomsRaw (Display *dpy, char **names, unsigned int nnames,
		       XmPlatBoolean only_if_exists, unsigned long *atoms)
{
    if (dpy == NULL || names == NULL || atoms == NULL) return ;
    XInternAtoms (dpy, names, (int) nnames,
		  only_if_exists ? True : False, (Atom *) atoms) ;
}

char *
_XmPlatAtomNameRaw (Display *dpy, unsigned long raw)
{
    if (dpy == NULL || raw == 0) return NULL ;
    return XGetAtomName (dpy, (Atom) raw) ;
}

/* Migration glue: see XmPlatP.h (Phase 4). */
extern Boolean _XmGetPointVisibility (Widget w, int root_x, int root_y) ;

Boolean
_XmPlatGetPointVisibilityX (Widget w, XmPlatEvent ev)
{
    return _XmGetPointVisibility (w,
				  _XmPlatEventRootX (ev),
				  _XmPlatEventRootY (ev)) ;
}

Boolean
_XmPlatGetPointVisibilityIsButton (Widget w, XEvent *event)
{
    XmPlatEvent pev ;

    if (event == NULL) return False ;
    pev = _XmPlatEventOf (event) ;
    if (! (_XmPlatEventIsButtonPress (pev) ||
	   _XmPlatEventIsButtonRelease (pev)))
	return False ;
    return _XmGetPointVisibility (w,
				  _XmPlatEventRootX (pev),
				  _XmPlatEventRootY (pev)) ;
}

/* ---- Atom / property contract (Phase 5) -------------------------------- */

struct _XmPlatAtomRec {
    Atom atom ;			/* interned server-side id */
    Display *dpy ;		/* display the atom was interned on */
    char *name ;		/* cached name (or NULL until asked) */
} ;

XmPlatAtom
_XmPlatAtomIntern (Display *dpy, const char *name,
		   XmPlatBoolean only_if_exists)
{
    XmPlatAtom a ;

    if (dpy == NULL || name == NULL) return NULL ;
    a = (XmPlatAtom) XtMalloc (sizeof (struct _XmPlatAtomRec)) ;
    a->atom = XInternAtom (dpy, name, only_if_exists ? True : False) ;
    a->dpy = dpy ;
    a->name = XtNewString (name) ;
    return a ;
}

const char *
_XmPlatAtomName (XmPlatAtom a)
{
    if (a == NULL) return NULL ;
    if (a->name == NULL && a->atom != None && a->dpy != NULL)
	a->name = XGetAtomName (a->dpy, a->atom) ;
    return a->name ;
}

unsigned long
_XmPlatAtomRaw (XmPlatAtom a)
{
    if (a == NULL) return 0 ;
    return (unsigned long) a->atom ;
}

XmPlatAtom
_XmPlatAtomOfRaw (Display *dpy, unsigned long raw)
{
    XmPlatAtom a ;

    a = (XmPlatAtom) XtMalloc (sizeof (struct _XmPlatAtomRec)) ;
    a->atom = (Atom) raw ;
    a->dpy = dpy ;
    a->name = NULL ;
    return a ;
}

void
_XmPlatAtomFree (XmPlatAtom a)
{
    if (a == NULL) return ;
    if (a->name) XtFree (a->name) ;
    XtFree ((char *) a) ;
}

void
_XmPlatChangeProperty (Display *dpy, unsigned long win,
		       unsigned long prop, unsigned long type,
		       int format, int mode,
		       const unsigned char *data, int nelements)
{
    XChangeProperty (dpy, (Window) win, (Atom) prop, (Atom) type,
		     format, mode, data, nelements) ;
}

int
_XmPlatGetWindowProperty (Display *dpy, unsigned long win,
			  unsigned long prop, long offset, long length,
			  XmPlatBoolean delete, unsigned long req_type,
			  unsigned long *ret_type, int *ret_format,
			  unsigned long *ret_nitems,
			  unsigned long *ret_bytes_after,
			  unsigned char **ret_data)
{
    Atom type ;
    int status ;
    unsigned long nitems, bytes_after ;

    status = XGetWindowProperty (dpy, (Window) win,
				 (Atom) prop,
				 offset, length,
				 delete ? True : False,
				 (Atom) req_type,
				 &type, ret_format, &nitems,
				 &bytes_after, ret_data) ;
    if (ret_type)
	*ret_type = (status == Success) ? (unsigned long) type : 0 ;
    if (ret_nitems) *ret_nitems = nitems ;
    if (ret_bytes_after) *ret_bytes_after = bytes_after ;
    return status ;
}

void
_XmPlatDeleteProperty (Display *dpy, unsigned long win, unsigned long prop)
{
    XDeleteProperty (dpy, (Window) win, (Atom) prop) ;
}

XmPlatBoolean
_XmPlatSendClientMessage (Display *dpy, unsigned long win,
			  XmPlatBoolean propagate, long event_mask,
			  const void *msg)
{
    return (XmPlatBoolean) XSendEvent (dpy, (Window) win,
				       propagate ? True : False,
				       event_mask, (XEvent *) msg) ;
}

void
_XmPlatRotateBuffers (Display *dpy, int n)
{
    XRotateBuffers (dpy, n) ;
}

void
_XmPlatSync (Display *dpy, XmPlatBoolean discard)
{
    XSync (dpy, discard ? True : False) ;
}
