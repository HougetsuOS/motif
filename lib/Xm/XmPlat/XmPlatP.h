/*
 * XmPlatP.h - internal (private) header for the XmPlat X11 backend.
 *
 * The seam in the other direction: Xm code that still holds a Display /
 * Drawable / GC builds an XmPlat handle on the stack with these inline
 * constructors during migration (Phase 1).  When Phase 1 completes the
 * constructors are only called from shell/display setup code, and when
 * the cairo backend lands (Phase 6) they change in one place.
 *
 * Private: include only from lib/Xm/XmPlat and Draw*.c.
 */
#ifndef XMPLATP_H
#define XMPLATP_H

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/Xft/Xft.h>
#include "XmPlat/XmPlatTypes.h"
#include "XmPlat/XmPlat.h"

/* lifecycle (internal) */
extern void _XmPlatSurfaceFree (XmPlatSurface surface) ;
extern void _XmPlatDrawCtxFree (XmPlatDrawCtx ctx) ;

struct _XmPlatSurfaceRec {
    Display     *dpy ;
    Drawable     d ;
    Visual      *visual ;
    int          depth ;
    Window       window ;	/* != None when the surface is a window */
};

struct _XmPlatFontRec {
    /* one of: XFontStruct* (kind 8/16), XFontSet (kind MB), XftFont*
       (kind XFT), NULL (kind GC - font id in fid) */
    void *f ;
    Font  fid ;       /* when kind == XmPlatTextGC */
    int   kind ;      /* XmPlatText8/16/MB/XFT/GC */
    Display *dpy ;    /* owner display; metrics prims need it for Xft */
};

struct _XmPlatDrawCtxRec {
    Display     *dpy ;
    GC           gc ;
    /* destination surface for prims */
    XmPlatSurface surface ;
    /* cached GC state; setters push lazily */
    unsigned long cached_mask ;
    XGCValues    cached ;
};

/* Constructors for migration (Phase 1).  Not part of the contract. */
extern XmPlatSurface _XmPlatSurfaceOf (Display *dpy, Drawable d,
				       Visual *visual, int depth,
				       Window window) ;
extern XmPlatDrawCtx _XmPlatDrawCtxOf (Display *dpy, GC gc) ;

/* Underlying drawable of a surface (backend seam; Phase-2 stipple
   comparison sites use it). */
extern Drawable _XmPlatSurfaceDrawable (XmPlatSurface surface) ;

#endif /* XMPLATP_H */

/*
 * Migration seam helpers: build a stack XmPlat handle from raw X11
 * objects that widget code still holds.  These exist ONLY so Phase-1
 * migration is mechanical; after Phase 1 only XmPlat internals call
 * them, and Phase 6 rewrites them in one place.
 */
static XmPlatSurface
_XmPlatSurface (Display *dpy, Drawable d)
{
    XmPlatSurface s = (XmPlatSurface) XtMalloc (sizeof (struct _XmPlatSurfaceRec)) ;
    s->dpy = dpy ; s->d = d ; s->visual = NULL ; s->depth = 0 ;
    s->window = None ;
    return s ;
}

static XmPlatDrawCtx
_XmPlatCtx (Display *dpy, Drawable d, GC gc)
{
    XmPlatDrawCtx c = (XmPlatDrawCtx) XtMalloc (sizeof (struct _XmPlatDrawCtxRec)) ;
    c->dpy = dpy ; c->gc = gc ; c->surface = _XmPlatSurface (dpy, d) ;
    c->cached_mask = 0 ;
    return c ;
}

static void
_XmPlatCtxFree (XmPlatDrawCtx c)
{
    if (c) { if (c->surface) XtFree ((char *) c->surface) ; XtFree ((char *) c) ; }
}

static XmPlatSurface
_XmPlatSurfaceOfWindow (Display *dpy, Window w)
{
    XmPlatSurface s ;

    s = (XmPlatSurface) XtMalloc (sizeof (struct _XmPlatSurfaceRec)) ;
    s->dpy = dpy ; s->d = w ; s->visual = NULL ; s->depth = 0 ;
    s->window = w ;
    return s ;
}

/* One-shot fill/draw helpers for migration sites that do a single prim. */
static void
_XmPlatFillOneRect (Display *dpy, Drawable d, GC gc,
		    int x, int y, unsigned int w, unsigned int h)
{
    XmPlatDrawCtx c = _XmPlatCtx (dpy, d, gc) ;
    _XmPlatFillRect (c, x, y, w, h) ;
    _XmPlatCtxFree (c) ;
}

static void
_XmPlatDrawOneLine (Display *dpy, Drawable d, GC gc,
		    int x1, int y1, int x2, int y2)
{
    XmPlatDrawCtx c = _XmPlatCtx (dpy, d, gc) ;
    _XmPlatDrawLine (c, x1, y1, x2, y2) ;
    _XmPlatCtxFree (c) ;
}

static void
_XmPlatClearOneRect (Display *dpy, Window w, int x, int y,
		     unsigned int width, unsigned int height)
{
    XmPlatSurface s = _XmPlatSurfaceOfWindow (dpy, w) ;
    _XmPlatClearArea (s, x, y, width, height) ;
    _XmPlatSurfaceFree (s) ;
}

static void
_XmPlatClrClip (Display *dpy, GC gc)
{
    XmPlatDrawCtx c = (XmPlatDrawCtx) XtMalloc (sizeof (struct _XmPlatDrawCtxRec)) ;
    c->dpy = dpy ; c->gc = gc ; c->surface = NULL ; c->cached_mask = 0 ;
    XSetClipMask (dpy, gc, None) ;
    XtFree ((char *) c) ;
}

/*
 * Phase-1 text seam: the GC's font (set via GCFont) is the font token.
 * The backend queries it at draw time.  Phase 2 replaces this with the
 * real XmFont contract.
 */
/* expose the underlying GC for widget-record GC fields (Phase 1 seam) */
static GC
_XmPlatGcOf (XmPlatDrawCtx c)
{
    return c->gc ;
}

/*
 * Image seam (Phase 3): the token is a struct; the static Phase-1 cast
 * is gone.  Use _XmPlatImageTokenOf at frozen-API boundaries; widget
 * code should hold XmPlatImage end-to-end.
 */
extern XmPlatImage _XmPlatImageTokenOf (XImage *ximage) ;
extern XImage *    _XmPlatImageXImage (XmPlatImage image) ;
/*
 * Release the token without destroying the underlying XImage (for
 * tokens built with _XmPlatImageTokenOf whose XImage outlives them).
 */
extern void        _XmPlatImageTokenFree (XmPlatImage image) ;
/* typed bitmap creator (the contract entry takes void* for X-freedom) */
extern XmPlatImage _XmPlatImageBitmapOf (Display *dpy, char *data,
					 unsigned int width,
					 unsigned int height) ;

/*
 * One-shot screen-to-image (XGetImage replacement for sites that hold
 * display + drawable, all planes, ZPixmap/XYBitmap by depth).  The
 * caller frees with _XmPlatImageFree.
 */
extern XmPlatImage _XmPlatImageFromSurface2 (Display *dpy, Drawable d,
					     int src_x, int src_y,
					     unsigned int w,
					     unsigned int h) ;

/*
 * Raw XImage constructor (ImageCache strip-scaler needs format + pad
 * control and then manages ->data itself).  Transitional seam.
 */
extern XImage *    _XmPlatImageRawCreate (Display *dpy, Visual *visual,
					  int depth, int format,
					  unsigned int w, unsigned int h,
					  int bitmap_pad) ;

/* font seam (Phase 1 escape hatch; Phase 2 replaces) */
extern XmPlatFont _XmPlatFontOfFontStruct (XFontStruct *fs) ;
extern XmPlatFont _XmPlatFontOfFontSet (XFontSet fs) ;
extern XmPlatFont _XmPlatFontOfGC (Display *dpy, GC gc) ;
/* Phase-2 font seam: wrap a modern-rendering font (XftFont). */
extern XmPlatFont _XmPlatFontOfXftFont (void *xftfont) ;
/*
 * Token builders that remember the owning Display (required for
 * Xft-kind tokens: XftTextExtents* needs it).  The Display-less
 * variants above stay for compatibility with existing seam callers;
 * new code should prefer these.
 */
extern XmPlatFont _XmPlatFontOfFontStructD (Display *dpy, XFontStruct *fs) ;
extern XmPlatFont _XmPlatFontOfFontSetD (Display *dpy, XFontSet fs) ;
extern XmPlatFont _XmPlatFontOfXftFontD (Display *dpy, void *xftfont) ;

/* Owner display of a font token (NULL if the token does not carry one). */
extern Display * _XmPlatFontDisplay (XmPlatFont font) ;

/*
 * Backend backing pointer of a token (XFontStruct, XFontSet, or XftFont).
 * Transitional seam for widgets whose records still store typed font
 * pointers; Phase 3 removes those fields.
 */
extern void * _XmPlatFontBacking (XmPlatFont font) ;

/*
 * First XFontStruct of a font-set token (the historic
 * "XFontsOfFontSet()[0]" extraction for the frozen
 * XmeRenderTableGetDefaultFont API).  NULL when not a font-set token.
 */
extern XFontStruct * _XmPlatFontSetFirstStruct (XmPlatFont font) ;

/* Internal seam (XmRenderT.c wrappers only): the backend's cached
   XftDraw for a surface.  Do not use from widget code. */
extern XftDraw * _XmPlatXftDrawOf (XmPlatDrawCtx ctx, XmPlatSurface surface) ;


/*
 * Event seam (Phase 4).  Widget code wraps the incoming XEvent* once at
 * the top of a handler and reads prims; frozen Xt/gadget signatures keep
 * XEvent* and unwrap via _XmPlatEventRaw where they must hand the raw
 * record onward.  None of these need Display.
 */

/* Window identity for event-field comparison ("is this event on my
   window?").  Widget -> XmPlatWindow. */
static XmPlatWindow
_XmPlatWindowOf (Widget w)
{
    return (XmPlatWindow) (w ? XtWindow (w) : 0) ;
}

/* Display behind an event token (event-plumbing use only: display-scoped
   caches and unique-event stamps; NOT for drawing). */
extern Display *_XmPlatEventDisplayOf (XmPlatEvent ev) ;

/* Pointer sub-kind test shorthand used by gadget dispatch code. */
#define _XmPlatEventIsButtonPress(ev) \
    (_XmPlatEventPointerKind (ev) == XmPlatButtonPress)
#define _XmPlatEventIsButtonRelease(ev) \
    (_XmPlatEventPointerKind (ev) == XmPlatButtonRelease)
#define _XmPlatEventIsMotion(ev) \
    (_XmPlatEventPointerKind (ev) == XmPlatPointerMotion)

/* Migration glue (Phase 4): the recurring "is this a button press or
   release whose root point is visible over the widget" test used by
   PushB/ToggleB/ToggleBG/ArrowB/DrawnB activate paths.  _XmGetPoint-
   Visibility is declared in TraversalI.h. */
extern Boolean _XmPlatGetPointVisibilityX (Widget w, XmPlatEvent ev) ;
extern Boolean _XmPlatGetPointVisibilityIsButton (Widget w, XEvent *event) ;
