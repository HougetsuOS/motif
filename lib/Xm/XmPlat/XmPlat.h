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

/* Put an image token into a surface (Phase-1 prim). */
extern void _XmPlatPutImage (XmPlatDrawCtx ctx, XmPlatImage img,
			     int src_x, int src_y,
			     int dst_x, int dst_y,
			     unsigned int w, unsigned int h) ;

/* ---- Image contract (Phase 3) --------------------------------------- */

/*
 * An image token is a client-side pixel buffer (in the X11 backend:
 * an XImage; in the cairo backend: an image surface).  Created once
 * by decoders/converters, drawn with _XmPlatPutImage, read/written
 * pixel-by-pixel with the prims below, freed when done.
 */

/*
 * Create an empty image of the given depth.  ctx supplies display,
 * visual and byte order.  The backend allocates the pixel buffer;
 * access it via _XmPlatImageData (packed rows, stride =
 * _XmPlatImageBytesPerLine) or write pixels via _XmPlatImagePutPixel.
 */
extern XmPlatImage  _XmPlatImageCreate (XmPlatDrawCtx ctx, int depth,
					unsigned int width,
					unsigned int height) ;

/* Variant with explicit visual (shells on non-default visuals). */
extern XmPlatImage  _XmPlatImageCreateOnVisual (XmPlatDrawCtx ctx,
						const void *visual,
						int depth,
						unsigned int width,
						unsigned int height) ;

/*
 * 1-bit bitmap from packed bit data (MSB-first scanlines, pad to byte
 * boundary), the classic "built-in bitmap" format.  data is owned by
 * the image afterwards.  Replaces the _XmCreateImage macro.  The
 * Display comes from the seam constructor (XmPlatP.h), which casts.
 */
extern XmPlatImage  _XmPlatImageCreateBitmap (void *display, char *data,
					      unsigned int width,
					      unsigned int height) ;

extern void         _XmPlatImageFree (XmPlatImage image) ;

/* Geometry + byte access. */
extern unsigned int _XmPlatImageWidth  (XmPlatImage image) ;
extern unsigned int _XmPlatImageHeight (XmPlatImage image) ;
extern int          _XmPlatImageDepth  (XmPlatImage image) ;
extern char *       _XmPlatImageData   (XmPlatImage image) ;
extern int          _XmPlatImageBytesPerLine (XmPlatImage image) ;

/* Pixel access. */
extern unsigned long _XmPlatImageGetPixel (XmPlatImage image, int x, int y) ;
extern void          _XmPlatImagePutPixel (XmPlatImage image, int x, int y,
					   unsigned long pixel) ;

/* Sub-image (shared scanline copy; free when done, not when parent is). */
extern XmPlatImage   _XmPlatImageSub (XmPlatImage image, int x, int y,
				      unsigned int w, unsigned int h) ;

/* Screen-to-image (replaces XGetImage).  ctx carries display + GC
   (plane mask); src is a window or pixmap surface. */
extern XmPlatImage   _XmPlatImageFromSurface (XmPlatDrawCtx ctx,
					      XmPlatSurface src,
					      int src_x, int src_y,
					      unsigned int w,
					      unsigned int h) ;

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

/* ---- Event contract (Phase 4) ---------------------------------------- */

/*
 * An event token wraps the platform event record for handler bodies.
 * In the X11/Xt backend it wraps an XEvent (identity on the record —
 * no copy) so that frozen Xt/gadget signatures can keep receiving
 * XEvent* while widget code reads only these prims.  A token is valid
 * for the duration of the handler call unless it is an *owning* token
 * made with _XmPlatEventCopy (used for event replay records).
 *
 * Event kinds mirror the platform's event families.  Field prims are
 * valid only for their family; "Other" events expose only the common
 * fields (kind, serial, send_event, time, window).
 */
enum {
    XmPlatEventNone,
    XmPlatEventPointer,		/* motion / button press / release */
    XmPlatEventKey,		/* key press / release */
    XmPlatEventCrossing,	/* enter / leave */
    XmPlatEventFocus,		/* focus in / out */
    XmPlatEventExpose,
    XmPlatEventGraphicsExpose,
    XmPlatEventNoExpose,
    XmPlatEventConfigure,
    XmPlatEventMap,
    XmPlatEventUnmap,
    XmPlatEventVisibility,
    XmPlatEventReparent,
    XmPlatEventProperty,
    XmPlatEventClientMessage,
    XmPlatEventSelection,
    XmPlatEventSelectionRequest,
    XmPlatEventSelectionClear,
    XmPlatEventColormap,
    XmPlatEventCirculate,
    XmPlatEventOther
} ;

typedef int XmPlatEventKind ;

/* Pointer sub-kind (X11: ButtonPress/ButtonRelease/MotionNotify).
   Values are disjoint from the XmPlatEventKind enum (base 100) so a
   single int can carry either without ambiguity. */
enum {
    XmPlatPointerMotion = 100,
    XmPlatButtonPress,
    XmPlatButtonRelease
} ;

/* Crossing / focus detail (X11 Notify* values). */
enum {
    XmPlatNotifyAncestor = 0,
    XmPlatNotifyVirtual,
    XmPlatNotifyInferior,
    XmPlatNotifyNonlinear,
    XmPlatNotifyNonlinearVirtual,
    XmPlatNotifyPointer,
    XmPlatNotifyPointerRoot,
    XmPlatNotifyDetailNone
} ;

/* Crossing modes. */
enum {
    XmPlatNotifyNormal = 0,
    XmPlatNotifyGrab,
    XmPlatNotifyUngrab
} ;

/* Visibility states. */
enum {
    XmPlatVisibilityUnobscured = 0,
    XmPlatVisibilityPartiallyObscured,
    XmPlatVisibilityFullyObscured
} ;

/* --- construction (migration seam) ----------------------------------- */

/* Wrap the raw event record for reading.  Never freed; dies with the
   raw record.  NULL raw event yields a token whose Kind is None. */
extern XmPlatEvent _XmPlatEventOf (const void *raw_event) ;

/* Owning copy for event replay records; release with
   _XmPlatEventFreeCopy.  NULL yields a None token. */
extern XmPlatEvent _XmPlatEventCopy (XmPlatEvent ev) ;
extern void        _XmPlatEventFreeCopy (XmPlatEvent ev) ;

/* Fabricate a token of the given kind carrying the field data of ev
   (gadget input-dispatch rewrite: same fields, new type).  The result
   is an owning copy; free with _XmPlatEventFreeCopy. */
extern XmPlatEvent _XmPlatEventSynth (XmPlatEvent ev, XmPlatEventKind kind) ;

/* The raw record behind a token, for passing through frozen Xt
   signatures (XtDispatchEvent, XmImMbLookupString, ...).  X11: XEvent*.
   Only meaningful on non-None tokens built from real records. */
extern void *_XmPlatEventRaw (XmPlatEvent ev) ;

/* --- common fields ---------------------------------------------------- */

extern XmPlatEventKind _XmPlatEventKind (XmPlatEvent ev) ;
extern XmPlatBoolean        _XmPlatEventIsType (XmPlatEvent ev,
					   XmPlatEventKind kind) ;
extern unsigned long   _XmPlatEventSerial (XmPlatEvent ev) ;
extern XmPlatBoolean        _XmPlatEventSendEvent (XmPlatEvent ev) ;
extern XmPlatTime      _XmPlatEventTime (XmPlatEvent ev) ;
extern XmPlatWindow    _XmPlatEventWindow (XmPlatEvent ev) ;

/* --- pointer family (motion / button) --------------------------------- */

extern int          _XmPlatEventX (XmPlatEvent ev) ;
extern int          _XmPlatEventY (XmPlatEvent ev) ;
extern int          _XmPlatEventRootX (XmPlatEvent ev) ;
extern int          _XmPlatEventRootY (XmPlatEvent ev) ;
extern unsigned int _XmPlatEventButton (XmPlatEvent ev) ;	/* 0 = none */
extern unsigned int _XmPlatEventState (XmPlatEvent ev) ;
extern XmPlatBoolean     _XmPlatEventIsHint (XmPlatEvent ev) ;
/* The sub-kind (motion/press/release); 0 for non-pointer events. */
extern int          _XmPlatEventPointerKind (XmPlatEvent ev) ;

/* Writable coordinate mutation (replay-prep clamping; e.g. TextIn
   DoGrabFocus).  Valid on pointer/key/crossing events. */
extern void _XmPlatEventSetX (XmPlatEvent ev, int x) ;
extern void _XmPlatEventSetY (XmPlatEvent ev, int y) ;

/* --- key family -------------------------------------------------------- */

extern unsigned int _XmPlatEventKeycode (XmPlatEvent ev) ;
/* Keysym for the keycode (X11: XLookupKeysym, col 0); NoSymbol when
   unmapped.  Phase-2 note: text translation stays with the IM layer. */
extern unsigned long _XmPlatEventKeysym (XmPlatEvent ev) ;

/* --- crossing family ---------------------------------------------------- */

extern XmPlatBoolean     _XmPlatEventFocus (XmPlatEvent ev) ;	/* has focus */
extern int          _XmPlatEventMode (XmPlatEvent ev) ;
extern int          _XmPlatEventDetail (XmPlatEvent ev) ;
extern XmPlatWindow _XmPlatEventSubwindow (XmPlatEvent ev) ;

/* Key direction (press vs release); False on non-key events. */
extern XmPlatBoolean _XmPlatEventIsKeyPress (XmPlatEvent ev) ;
extern XmPlatBoolean _XmPlatEventIsKeyRelease (XmPlatEvent ev) ;

/* Crossing direction (enter vs leave); False on non-crossing events. */
extern XmPlatBoolean _XmPlatEventIsEnter (XmPlatEvent ev) ;
extern XmPlatBoolean _XmPlatEventIsLeave (XmPlatEvent ev) ;
/* Focus direction (in vs out); False on non-focus events. */
extern XmPlatBoolean _XmPlatEventIsFocusIn (XmPlatEvent ev) ;
extern XmPlatBoolean _XmPlatEventIsFocusOut (XmPlatEvent ev) ;

/* --- expose / configure family ------------------------------------------ */

extern unsigned int _XmPlatEventWidth (XmPlatEvent ev) ;
extern unsigned int _XmPlatEventHeight (XmPlatEvent ev) ;
extern unsigned int _XmPlatEventCount (XmPlatEvent ev) ;	/* expose */
extern unsigned int _XmPlatEventBorderWidth (XmPlatEvent ev) ;
extern int          _XmPlatEventVisibilityState (XmPlatEvent ev) ;

/* --- property / client-message family (opaque payloads; Phase 5) -------- */

/* Property atom as a string (backend interns the name); NULL when the
   event carries none. */
extern const char *_XmPlatEventPropertyName (XmPlatEvent ev) ;
/* Selection requestor window (XSelectionRequestEvent). */
extern XmPlatWindow _XmPlatEventRequestor (XmPlatEvent ev) ;
