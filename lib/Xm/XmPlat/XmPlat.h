/*
 * XmPlat - Motif platform contract (plan: doc/plat-abstraction.md)
 *
 * This header is the Phase-1 draw-primitive contract.  No X11 types
 * (Display *, Drawable, GC, Window, XEvent, Font) appear in any
 * signature.  Backends are selected at link time; exactly one
 * implementation of this contract is linked into libXm.
 *
 * Rules (plan §2.2):
 *   - one-way dependency: lib/Xm -> lib/XmPlat -> (backend) -> X11/Xt
 *   - XmPlat headers must not include Xm.h or X11/Intrinsic.h
 *   - nothing enters the contract that the X11 backend does not use
 */
#ifndef XMPLAT_H
#define XMPLAT_H

#include <XmPlat/XmPlatTypes.h>

/* ---- Draw context attribute setters ------------------------------- */

/* Foreground pixel for subsequent prims. */
extern void _XmPlatSetForeground (XmPlatDrawCtx ctx, XmPlatPixel fg) ;

/* Line width in pixels (0 means "fastest thin line"). */
extern void _XmPlatSetLineWidth (XmPlatDrawCtx ctx, unsigned int width) ;
/* Read back the current line width (for save/restore around prims). */
extern unsigned int _XmPlatGetLineWidth (XmPlatDrawCtx ctx) ;

/* Line attributes bundle (width, style, cap, join) for save/restore. */
typedef struct {
    unsigned int width ;
    int          style ;
    int          cap ;
    int          join ;
} XmPlatLineAttr ;

extern void          _XmPlatSetLineAttr (XmPlatDrawCtx ctx,
					  const XmPlatLineAttr *la) ;
extern XmPlatLineAttr _XmPlatGetLineAttr (XmPlatDrawCtx ctx) ;

/* Line style: XmPlatLineSolid or XmPlatLineOnOffDash. */
extern void _XmPlatSetLineStyle (XmPlatDrawCtx ctx, int style) ;

/* Dash pattern (npatterns entries, length in pixels). */
extern void _XmPlatSetDashes (XmPlatDrawCtx ctx,
			      const unsigned char *dash_list,
			      int ndash, unsigned int offset) ;

/* Clip to rectangle (x, y, w, h); w or h == 0 removes clipping. */
extern void _XmPlatSetClipRect (XmPlatDrawCtx ctx,
				int x, int y,
				unsigned int w, unsigned int h) ;

/* Clip to a 1-bit mask surface with origin at (x, y). */
extern void _XmPlatSetClipMask (XmPlatDrawCtx ctx, XmPlatSurface mask,
				int x, int y) ;

/* Flush pending output. */
extern void _XmPlatFlush (XmPlatDrawCtx ctx) ;

/* Clear the whole surface (expose-erase semantics). */
extern void _XmPlatClearWindow (XmPlatSurface surface) ;

/* Put an XImage-derived buffer (Phase 1: opaque image token built at the
   seam; Phase 3 replaces with XmImage). */
typedef struct _XmPlatImageRec *XmPlatImage ;
extern void _XmPlatPutImage (XmPlatDrawCtx ctx, XmPlatImage img,
			     int src_x, int src_y,
			     int dst_x, int dst_y,
			     unsigned int w, unsigned int h) ;

/* Clip against a mask surface and restore. */
extern void _XmPlatSetClipMaskSurf (XmPlatDrawCtx ctx, XmPlatSurface mask,
				    int x, int y) ;

/* Set only the clip origin, keeping the current clip mask. */
extern void _XmPlatSetClipOrigin (XmPlatDrawCtx ctx, int x, int y) ;

/* Background pixel attribute. */
extern void _XmPlatSetBackground (XmPlatDrawCtx ctx, XmPlatPixel bg) ;

/* Read back the current foreground/background pixel and the GC's font
   id (0 when none).  Replaces direct XGetGCValues calls in widget
   code; the fields are the only GC state callers need to inspect. */
extern XmPlatPixel _XmPlatGetForeground (XmPlatDrawCtx ctx) ;
extern XmPlatPixel _XmPlatGetBackground (XmPlatDrawCtx ctx) ;
extern unsigned long _XmPlatGetFontId (XmPlatDrawCtx ctx) ;
/* Stipple pixmap id of the ctx GC (None when not stippled).  The
   token is a backend surface; NULL means no stipple. */
extern XmPlatSurface _XmPlatGetStipple (XmPlatDrawCtx ctx) ;

/* Tile (2..32bpp pixmap surface) / stipple (1-bit) + origin for fill. */
extern void _XmPlatSetTile   (XmPlatDrawCtx ctx, XmPlatSurface tile) ;
extern void _XmPlatSetStipple(XmPlatDrawCtx ctx, XmPlatSurface stipple,
			       int ts_x_origin, int ts_y_origin) ;

/* ---- Draw primitives --------------------------------------------- */

extern void _XmPlatDrawPoint  (XmPlatDrawCtx ctx, int x, int y) ;
extern void _XmPlatDrawPoints (XmPlatDrawCtx ctx, const XmPlatPoint *pts,
			       int npts, int relative) ;
extern void _XmPlatDrawLine   (XmPlatDrawCtx ctx,
			       int x1, int y1, int x2, int y2) ;
extern void _XmPlatDrawLines  (XmPlatDrawCtx ctx, const XmPlatPoint *pts,
			       int npts, int relative) ;
extern void _XmPlatDrawSegments (XmPlatDrawCtx ctx,
				 const XmPlatSegment *segs, int nsegs) ;
extern void _XmPlatDrawRect   (XmPlatDrawCtx ctx,
			       int x, int y, unsigned int w, unsigned int h) ;
extern void _XmPlatDrawRects  (XmPlatDrawCtx ctx, const XmPlatRect *rects,
			       int nrects) ;
extern void _XmPlatDrawArc    (XmPlatDrawCtx ctx,
			       int x, int y, unsigned int w, unsigned int h,
			       int angle1, int angle2) ;
extern void _XmPlatFillRect   (XmPlatDrawCtx ctx,
			       int x, int y, unsigned int w, unsigned int h) ;
extern void _XmPlatFillRects  (XmPlatDrawCtx ctx, const XmPlatRect *rects,
			       int nrects) ;
extern void _XmPlatFillPolygon(XmPlatDrawCtx ctx, const XmPlatPoint *pts,
			       int npts, int convex) ;
extern void _XmPlatFillArc    (XmPlatDrawCtx ctx,
			       int x, int y, unsigned int w, unsigned int h,
			       int angle1, int angle2) ;
extern void _XmPlatFillRectangleTiled (XmPlatDrawCtx ctx,
			       int x, int y, unsigned int w, unsigned int h) ;
extern void _XmPlatFillRectangleStippled (XmPlatDrawCtx ctx,
			       int x, int y, unsigned int w, unsigned int h) ;
extern void _XmPlatBlit (XmPlatDrawCtx ctx, XmPlatSurface src,
			 int src_x, int src_y,
			 int dst_x, int dst_y,
			 unsigned int w, unsigned int h) ;
extern void _XmPlatBlitMask (XmPlatDrawCtx ctx, XmPlatSurface src,
			     XmPlatSurface mask,
			     int src_x, int src_y,
			     int dst_x, int dst_y,
			     unsigned int w, unsigned int h) ;
extern void _XmPlatClearArea (XmPlatSurface surface,
			      int x, int y, unsigned int w, unsigned int h) ;

#endif /* XMPLAT_H */

/* ---- GC state (Phase 1 escape hatch) ------------------------------ */

/*
 * Full XGCValues change/create operations.  The mask/values are X11
 * bit encodings *at the seam*; callers construct them in the few
 * remaining sites until Phase 2 moves GC management behind XmPlat.
 * (XGCValues is passed through opaquely as a blob; the backend casts.)
 */
extern void _XmPlatChangeGCValues (XmPlatDrawCtx ctx, unsigned long value_mask,
				   const void *values) ;
extern XmPlatDrawCtx _XmPlatCreateCtxOnSurface (XmPlatSurface surface,
						unsigned long value_mask,
						const void *values) ;

/* ---- Raw text prims (Phase 1 escape hatch) ------------------------ */

/*
 * Text drawing in Motif is font-mediated; the full XmFont contract
 * lands in Phase 2 (plan §3).  Until then these prims carry an opaque
 * font token built at the seam (XmPlatP.h) so widgets can stop calling
 * XDrawString/XmbDrawString directly.  The backend casts the token
 * back to XFontStruct * / XFontSet.
 */
extern void _XmPlatDrawString (XmPlatDrawCtx ctx, XmPlatFont font,
			       int kind, const void *text, int len,
			       int x, int y, int image) ;

/* ---- Font contract (Phase 2) --------------------------------------- */

/*
 * A font token is created once (at rendition creation / widget font
 * setup) and handed to the prims below plus _XmPlatDrawString.  All
 * metrics answers are integers in pixels, matching what the core-X
 * structures expose.  A token is valid for the Display it was built
 * for; the backend is free to keep a lazily-loaded representation.
 */

/* Kind introspection (XmPlatText8/16/MB/XFT/GC; WC/UTF8 never appear
   as token kinds - they are text kinds). */
extern int _XmPlatFontKind (XmPlatFont font) ;

/* Release a font token (does not unload the underlying font; tokens
   are cheap wrappers). */
extern void _XmPlatFontFree (XmPlatFont font) ;

/*
 * Load (and later release) a core font by XLFD name.  Answers a
 * token for the loaded font, or NULL when the name cannot be loaded.
 * The token owns the font; _XmPlatFontUnload frees it.  (Legacy
 * widgets stash the returned token in XFontStruct-typed fields via
 * the backend seam until Phase 3 removes those fields.)
 */
extern XmPlatFont _XmPlatFontLoad (Display *dpy, const char *name) ;
extern void _XmPlatFontUnload (XmPlatFont font) ;

/* Line metrics. */
extern int _XmPlatFontAscent  (XmPlatFont font) ;
extern int _XmPlatFontDescent (XmPlatFont font) ;
extern int _XmPlatFontHeight  (XmPlatFont font) ;
/* Average advance of a representative digit run; falls back to
   max-advance (Xft) or min/max midpoint (core fonts). */
extern int _XmPlatFontAverageWidth (XmPlatFont font) ;

/*
 * String metrics.  kind selects the text encoding (XmPlatText8/16/MB/
 * WC/UTF8); text/len are the string and its element count (bytes for
 * 8/MB/UTF8, XChar2b units for 16, wchar_t units for WC).  overall
 * receives ink/advance extents; it may be NULL when only the advance
 * width is wanted.
 */
extern int  _XmPlatTextWidth (XmPlatFont font, int kind,
			      const void *text, int len) ;
extern void _XmPlatTextExtents (XmPlatFont font, int kind,
				const void *text, int len,
				XmPlatCharInfo *overall) ;
/* Width of the single glyph/string "0" times n - tab-stop helper for
   the backend renderers whose notion of a default digit advance
   differs from XTextWidth semantics. */
extern int _XmPlatDigitWidth (XmPlatFont font, int n) ;

/*
 * Text drawing with explicit foreground color for rendering backends.
 * The core-X backend uses color->pixel through the GC; the kind/len
 * rules are the same as _XmPlatTextWidth.  A NULL color means "use
 * the ctx GC foreground" (the Phase-2 transitional behavior).
 */
extern void _XmPlatDrawStringColored (XmPlatDrawCtx ctx, XmPlatFont font,
				      int kind, const void *text, int len,
				      int x, int y, int image,
				      const XmPlatColor *color) ;
