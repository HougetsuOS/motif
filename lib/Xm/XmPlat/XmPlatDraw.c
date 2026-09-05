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
    XSetForeground (ctx->dpy, ctx->gc, fg) ;
}

void
_XmPlatSetLineWidth (XmPlatDrawCtx ctx, unsigned int width)
{
    XGCValues v ;
    unsigned long mask = GCLineWidth ;
    v.line_width = (int)width ;
    XChangeGC (ctx->dpy, ctx->gc, mask, &v) ;
}

void
_XmPlatSetLineAttr (XmPlatDrawCtx ctx, const XmPlatLineAttr *la)
{
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

    XGetGCValues (ctx->dpy, ctx->gc, GCLineWidth, &v) ;
    return (unsigned int) v.line_width ;
}

void
_XmPlatSetLineStyle (XmPlatDrawCtx ctx, int style)
{
    XGCValues v ;
    unsigned long mask = GCLineStyle ;
    v.line_style = (style == XmPlatLineOnOffDash)? LineOnOffDash :
		   (style == XmPlatLineDoubleDash)? LineDoubleDash : LineSolid ;
    XChangeGC (ctx->dpy, ctx->gc, mask, &v) ;
}

void
_XmPlatSetDashes (XmPlatDrawCtx ctx, const unsigned char *dash_list,
		  int ndash, unsigned int offset)
{
    XSetDashes (ctx->dpy, ctx->gc, (int)offset, (const char *) dash_list, ndash) ;
}

void
_XmPlatSetClipRect (XmPlatDrawCtx ctx, int x, int y,
		    unsigned int w, unsigned int h)
{
    XRectangle r ;

    if (ctx->gc == NULL) return ;	/* Xft-draw-only ctx */
    if (w == 0 || h == 0) {
	XSetClipMask (ctx->dpy, ctx->gc, None) ;
	return ;
    }
    r.x = (short)x ; r.y = (short)y ;
    r.width = (unsigned short)w ; r.height = (unsigned short)h ;
    XSetClipRectangles (ctx->dpy, ctx->gc, 0, 0, &r, 1, Unsorted) ;
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
    XSetBackground (ctx->dpy, ctx->gc, bg) ;
}

XmPlatPixel
_XmPlatGetForeground (XmPlatDrawCtx ctx)
{
    XGCValues v ;

    XGetGCValues (ctx->dpy, ctx->gc, GCForeground, &v) ;
    return v.foreground ;
}

XmPlatPixel
_XmPlatGetBackground (XmPlatDrawCtx ctx)
{
    XGCValues v ;

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
    XPutImage (ctx->dpy, ctx->surface->d, ctx->gc,
	       (XImage *) image, src_x, src_y, dst_x, dst_y, w, h) ;
}

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
