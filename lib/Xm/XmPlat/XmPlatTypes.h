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

/* Fill styles */
#define XmPlatFillSolid      0
#define XmPlatFillTiled      1
#define XmPlatFillStippled   2
#define XmPlatFillOpaqueStippled 3

#endif /* XMPLAT_TYPES_H */
