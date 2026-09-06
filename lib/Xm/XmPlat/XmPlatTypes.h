/*
 * XmPlat - opaque handle types for the Motif platform contract.
 *
 * Handles are X11 objects in the current backend; the rest of Xm must
 * treat them as opaque values.  (Phase 1: draw prims only; font, image
 * and event handles join the contract in Phases 2-4.)
 */
#ifndef XMPLAT_TYPES_H
#define XMPLAT_TYPES_H

typedef struct _XmPlatSurfaceRec   *XmPlatSurface ;
typedef struct _XmPlatDrawCtxRec   *XmPlatDrawCtx ;
typedef struct _XmPlatFontRec      *XmPlatFont ;
typedef struct _XmPlatImageRec     *XmPlatImage ;
typedef struct _XmPlatEventRec     *XmPlatEvent ;

/*
 * Window token (Phase 4): identifies a window for event-field purposes
 * ("did this event happen on my window?").  X11: the Window id cast to
 * a pointer-sized integer; other backends define their own encoding.
 */
typedef unsigned long XmPlatWindow ;
#define XmPlatWindowNone ((XmPlatWindow) 0)

/* Timestamp in event fields (X11 Time; 0 == CurrentTime). */
typedef unsigned long XmPlatTime ;

#ifndef XMPLAT_BOOLEAN
#define XMPLAT_BOOLEAN
typedef int XmPlatBoolean ;
#ifndef True
#define True  1
#define False 0
#endif
#endif

typedef unsigned long XmPlatPixel ;
typedef int           XmPlatAngle ;	/* 64ths of a degree, X11 units */

typedef struct {
    int x, y ;
} XmPlatPoint ;

typedef struct {
    int          x1, y1, x2, y2 ;
} XmPlatSegment ;

typedef struct {
    int          x, y ;
    unsigned int width, height ;
} XmPlatRect ;

/* Line styles */
#define XmPlatLineSolid     0
#define XmPlatLineOnOffDash 1
#define XmPlatLineDoubleDash 2

/* Text kinds */
enum {
    XmPlatText8,   /* char *            (XFontStruct path)   */
    XmPlatText16,  /* unsigned short *  (XFontStruct 2-byte) */
    XmPlatTextMB,  /* multibyte char *  (XFontSet path)      */
    XmPlatTextWC   /* wide char *       (XFontSet path)      */
} ;

/*
 * Additional kind codes shared by font tokens and text-kind arguments.
 * XmPlatTextGC marks a font token whose font rides in the GC (Phase-1
 * shim); XmPlatTextXFT marks a font token holding a backend font of
 * the modern-rendering family; XmPlatTextUTF8/32 extend the text-kind
 * argument of the string prims.
 */
#define XmPlatTextGC   4
#define XmPlatTextXFT  5
#define XmPlatTextUTF8 6
#define XmPlatText32   7

/*
 * Char metrics, field-for-field equivalent of the core-X char struct.
 * Filled by _XmPlatTextExtents; lbearing/rbearing are ink offsets from
 * the origin, width is the advance, ascent/descent the ink extents.
 */
typedef struct {
    short lbearing, rbearing, width ;
    short ascent, descent ;
    unsigned short attributes ;
} XmPlatCharInfo ;

/*
 * Color for text/image prims on rendering backends that need explicit
 * color (alpha-capable).  pixel is the colormap index when one exists;
 * red/green/blue/alpha are 16-bit components.  Core-X backends use
 * pixel; cairo backends use the components.
 */
typedef struct {
    unsigned long  pixel ;
    unsigned short red, green, blue, alpha ;
} XmPlatColor ;

/* Fill styles */
#define XmPlatFillSolid      0
#define XmPlatFillTiled      1
#define XmPlatFillStippled   2
#define XmPlatFillOpaqueStippled 3

#endif /* XMPLAT_TYPES_H */
