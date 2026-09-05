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
    return f ;
}

/* --- text prims (Phase 1 escape hatch) ----------------------------- */

XmPlatFont
_XmPlatFontOfFontStruct (XFontStruct *fs)
{
    XmPlatFont f = (XmPlatFont) XtMalloc (sizeof (struct _XmPlatFontRec)) ;

    f->f = fs ;
    f->kind = XmPlatText8 ;
    return f ;
}

XmPlatFont
_XmPlatFontOfFontSet (XFontSet fs)
{
    XmPlatFont f = (XmPlatFont) XtMalloc (sizeof (struct _XmPlatFontRec)) ;

    f->f = fs ;
    f->kind = XmPlatTextMB ;
    return f ;
}

void
_XmPlatDrawString (XmPlatDrawCtx ctx, XmPlatFont font, int kind,
		   const void *text, int len, int x, int y, int image)
{
    Drawable d = ctx->surface->d ;

    if (font->kind == XmPlatTextGC) {
        /* font rides in the GC; kind selects 8/16/mb by the call site */
        if (kind == XmPlatText16) {
            if (image)
                XDrawImageString16 (ctx->dpy, d, ctx->gc,
                                    x, y, (XChar2b *) text, len) ;
            else
                XDrawString16 (ctx->dpy, d, ctx->gc,
                               x, y, (XChar2b *) text, len) ;
        } else {
            if (image)
                XDrawImageString (ctx->dpy, d, ctx->gc, x, y,
                                  (const char *) text, len) ;
            else
                XDrawString (ctx->dpy, d, ctx->gc, x, y,
                             (const char *) text, len) ;
        }
        return ;
    }

    if (font->kind == XmPlatText8 || font->kind == XmPlatText16) {
	XFontStruct *fs = (XFontStruct *) font->f ;
	if (kind == XmPlatText16) {
	    if (image)
		XDrawImageString16 (ctx->dpy, d, ctx->gc,
				    x, y, (XChar2b *) text, len) ;
	    else
		XDrawString16 (ctx->dpy, d, ctx->gc,
			       x, y, (XChar2b *) text, len) ;
	} else {
	    if (image)
		XDrawImageString (ctx->dpy, d, ctx->gc, x, y,
				  (const char *) text, len) ;
	    else
		XDrawString (ctx->dpy, d, ctx->gc, x, y,
			     (const char *) text, len) ;
	}
    } else {
	XFontSet fset = (XFontSet) font->f ;
	if (kind == XmPlatTextWC) {
	    if (image)
		XwcDrawImageString (ctx->dpy, d, fset, ctx->gc, x, y,
				    (wchar_t *) text, len) ;
	    else
		XwcDrawString (ctx->dpy, d, fset, ctx->gc, x, y,
			       (wchar_t *) text, len) ;
	} else {
	    if (image)
		XmbDrawImageString (ctx->dpy, d, fset, ctx->gc, x, y,
				    (const char *) text, len) ;
	    else
		XmbDrawString (ctx->dpy, d, fset, ctx->gc, x, y,
			       (const char *) text, len) ;
	}
    }
}
