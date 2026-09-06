/*
 * primtest - Phase-7 headless verification of the XmPlat render contract
 * (doc/plat-abstraction.md §3 Phase 7).
 *
 * Runs with no X server: the draw contexts are cairo image surfaces
 * created by _XmPlatMemCtxCreate; the vector prims + attribute setters
 * of the cairo backend drive them through the same contract entry
 * points the widgets use.  Each subtest probes the pixel buffer at
 * coordinates the prim must have painted (with a small tolerance for
 * cairo's antialiasing of endpoints) and fails loudly otherwise.
 *
 * Compiled by tools/gate/p7-memory-gate.sh (not part of libXm).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "XmPlat/XmPlatP.h"

static int failures = 0 ;

/* Stub: the real one lives in lib/Xm/Traversal.c (Xt/X11-bound); the
   memory backend never exercises it. */
Boolean _XmGetPointVisibility (Widget w, int root_x, int root_y)
{ (void) w ; (void) root_x ; (void) root_y ; return False ; }

static void
Probe (XmPlatDrawCtx c, const char *what, int x, int y,
       int want_r, int want_g, int want_b, int tol)
{
    int stride ;
    unsigned char *data = _XmPlatMemCtxData (c, &stride) ;
    unsigned char *px ;
    int r, g, b ;

    if (data == NULL) {
	printf ("FAIL %s: no pixel data\n", what) ;
	failures++ ;
	return ;
    }
    px = data + y * stride + x * 4 ;
    /* ARGB32 little-endian layout: B, G, R, A (premultiplied) */
    b = px[0] ; g = px[1] ; r = px[2] ;
    if (abs (r - want_r) > tol || abs (g - want_g) > tol ||
	abs (b - want_b) > tol) {
	printf ("FAIL %s: (%d,%d) got rgb(%d,%d,%d), want rgb(%d,%d,%d)\n",
		what, x, y, r, g, b, want_r, want_g, want_b) ;
	failures++ ;
    }
}

static void
ClearToWhite (XmPlatDrawCtx c)
{
    int stride, w, h, x, y ;
    unsigned char *data ;

    w = _XmPlatMemCtxWidth (c) ;
    h = _XmPlatMemCtxHeight (c) ;
    data = _XmPlatMemCtxData (c, &stride) ;
    for (y = 0 ; y < h ; y++)
	for (x = 0 ; x < w ; x++) {
	    unsigned char *px = data + y * stride + x * 4 ;
	    px[0] = px[1] = px[2] = 0xFF ;
	    px[3] = 0xFF ;
	}
    _XmPlatMemCtxMarkDirty (c) ;
}

static void
test_fill_rect (void)
{
    XmPlatDrawCtx c = _XmPlatMemCtxCreate (64, 64) ;

    ClearToWhite (c) ;
    _XmPlatSetForeground (c, 0xFF0000) ;		/* red */
    _XmPlatFillRect (c, 10, 10, 20, 20) ;
    Probe (c, "fillrect-mid", 20, 20, 255, 0, 0, 0) ;
    Probe (c, "fillrect-corner", 11, 11, 255, 0, 0, 0) ;
    Probe (c, "fillrect-outside", 40, 40, 255, 255, 255, 0) ;
    _XmPlatMemCtxFree (c) ;
    printf ("fill-rect: %s\n", failures ? "FAIL" : "ok") ;
}

static void
test_draw_rect (void)
{
    XmPlatDrawCtx c = _XmPlatMemCtxCreate (64, 64) ;

    ClearToWhite (c) ;
    _XmPlatSetForeground (c, 0x0000FF) ;		/* blue */
    _XmPlatSetLineWidth (c, 2) ;
    _XmPlatDrawRect (c, 5, 5, 30, 30) ;
    Probe (c, "drawrect-top", 20, 5, 0, 0, 255, 40) ;
    Probe (c, "drawrect-left", 5, 20, 0, 0, 255, 40) ;
    Probe (c, "drawrect-inside", 20, 20, 255, 255, 255, 0) ;
    _XmPlatMemCtxFree (c) ;
    printf ("draw-rect: %s\n", failures ? "FAIL" : "ok") ;
}

static void
test_draw_line (void)
{
    XmPlatDrawCtx c = _XmPlatMemCtxCreate (64, 64) ;

    ClearToWhite (c) ;
    _XmPlatSetForeground (c, 0x00FF00) ;		/* green */
    _XmPlatDrawLine (c, 5, 10, 55, 10) ;		/* horizontal */
    Probe (c, "line-mid", 30, 10, 0, 255, 0, 40) ;
    Probe (c, "line-above", 30, 6, 255, 255, 255, 0) ;
    _XmPlatMemCtxFree (c) ;
    printf ("draw-line: %s\n", failures ? "FAIL" : "ok") ;
}

static void
test_clip (void)
{
    XmPlatDrawCtx c = _XmPlatMemCtxCreate (64, 64) ;

    ClearToWhite (c) ;
    _XmPlatSetClipRect (c, 10, 10, 20, 20) ;
    _XmPlatSetForeground (c, 0xFF0000) ;
    _XmPlatFillRect (c, 0, 0, 64, 64) ;			/* crosses the clip */
    Probe (c, "clip-in", 20, 20, 255, 0, 0, 0) ;
    Probe (c, "clip-out", 40, 40, 255, 255, 255, 0) ;
    /* remove the clip */
    _XmPlatSetClipRect (c, 0, 0, 0, 0) ;
    ClearToWhite (c) ;
    _XmPlatFillRect (c, 0, 0, 64, 64) ;
    Probe (c, "clip-cleared", 40, 40, 255, 0, 0, 0) ;
    _XmPlatMemCtxFree (c) ;
    printf ("clip: %s\n", failures ? "FAIL" : "ok") ;
}

static void
test_dash (void)
{
    XmPlatDrawCtx c = _XmPlatMemCtxCreate (64, 64) ;

    ClearToWhite (c) ;
    _XmPlatSetForeground (c, 0x000000) ;
    _XmPlatSetLineStyle (c, XmPlatLineOnOffDash) ;
    {
	/* 4 on, 4 off */
	unsigned char dl[2] = { 4, 4 } ;
	_XmPlatSetDashes (c, dl, 2, 0) ;
    }
    _XmPlatDrawLine (c, 0, 30, 63, 30) ;
    /* x=2 is inside the first 4-pixel dash (cairo half-pixel shift:
       pattern origin at x=0 → dash runs [0,4) on the path, pixels
       near x=2 get painted). */
    Probe (c, "dash-on", 2, 30, 0, 0, 0, 40) ;
    /* x=6 is inside the 4-pixel gap */
    Probe (c, "dash-off", 6, 30, 255, 255, 255, 0) ;
    _XmPlatMemCtxFree (c) ;
    printf ("dash: %s\n", failures ? "FAIL" : "ok") ;
}

static void
test_fill_arc (void)
{
    XmPlatDrawCtx c = _XmPlatMemCtxCreate (64, 64) ;

    ClearToWhite (c) ;
    _XmPlatSetForeground (c, 0x808080) ;
    /* 90..180 degrees: X11 angles from 3 o'clock; 90*64 = 6 o'clock
       (positive = counterclockwise on paper, which is clockwise on
       screen), sweeping 90*64 more to 9 o'clock (bottom-left quadrant
       on screen).  Center of arc bounding box (32,32), radius 20. */
    _XmPlatFillArc (c, 12, 12, 40, 40, 90 * 64, 90 * 64) ;
    Probe (c, "fillarc-interior", 26, 44, 128, 128, 128, 2) ;
    Probe (c, "fillarc-interior2", 16, 38, 128, 128, 128, 2) ;
    Probe (c, "fillarc-notright", 51, 32, 255, 255, 255, 0) ;
    Probe (c, "fillarc-nottop", 32, 13, 255, 255, 255, 0) ;
    _XmPlatMemCtxFree (c) ;
    printf ("fill-arc: %s\n", failures ? "FAIL" : "ok") ;
}

static void
test_fill_polygon (void)
{
    XmPlatDrawCtx c = _XmPlatMemCtxCreate (64, 64) ;
    XmPlatPoint tri[3] ;

    ClearToWhite (c) ;
    tri[0].x = 32 ; tri[0].y = 5 ;
    tri[1].x = 5 ;  tri[1].y = 55 ;
    tri[2].x = 59 ; tri[2].y = 55 ;
    _XmPlatSetForeground (c, 0x404040) ;
    _XmPlatFillPolygon (c, tri, 3, 1) ;
    Probe (c, "polygon-inside", 32, 45, 64, 64, 64, 40) ;
    Probe (c, "polygon-outside", 5, 15, 255, 255, 255, 0) ;
    _XmPlatMemCtxFree (c) ;
    printf ("fill-polygon: %s\n", failures ? "FAIL" : "ok") ;
}

static void
test_segments (void)
{
    XmPlatDrawCtx c = _XmPlatMemCtxCreate (64, 64) ;
    XmPlatSegment segs[2] ;

    ClearToWhite (c) ;
    segs[0].x1 = 5 ; segs[0].y1 = 20 ; segs[0].x2 = 55 ; segs[0].y2 = 20 ;
    segs[1].x1 = 5 ; segs[1].y1 = 40 ; segs[1].x2 = 55 ; segs[1].y2 = 40 ;
    _XmPlatSetForeground (c, 0x000080) ;
    _XmPlatDrawSegments (c, segs, 2) ;
    Probe (c, "seg0", 30, 20, 0, 0, 128, 40) ;
    Probe (c, "seg1", 30, 40, 0, 0, 128, 40) ;
    Probe (c, "seg-between", 30, 30, 255, 255, 255, 0) ;
    _XmPlatMemCtxFree (c) ;
    printf ("segments: %s\n", failures ? "FAIL" : "ok") ;
}

static void
test_points (void)
{
    XmPlatDrawCtx c = _XmPlatMemCtxCreate (64, 64) ;
    XmPlatPoint pts[2] ;

    ClearToWhite (c) ;
    pts[0].x = 10 ; pts[0].y = 10 ;
    pts[1].x = 50 ; pts[1].y = 50 ;
    _XmPlatSetForeground (c, 0xFF00FF) ;
    _XmPlatDrawPoints (c, pts, 2, 0) ;
    Probe (c, "point0", 10, 10, 255, 0, 255, 0) ;
    Probe (c, "point1", 50, 50, 255, 0, 255, 0) ;
    Probe (c, "point-none", 30, 30, 255, 255, 255, 0) ;
    _XmPlatMemCtxFree (c) ;
    printf ("points: %s\n", failures ? "FAIL" : "ok") ;
}

static void
test_attr_readback (void)
{
    XmPlatDrawCtx c = _XmPlatMemCtxCreate (8, 8) ;
    XmPlatLineAttr la ;

    _XmPlatSetForeground (c, 0x123456) ;
    if (_XmPlatGetForeground (c) != 0x123456) {
	printf ("FAIL attr-fg readback: 0x%lx\n",
		(unsigned long) _XmPlatGetForeground (c)) ;
	failures++ ;
    }
    _XmPlatSetBackground (c, 0xABCDEF) ;
    if (_XmPlatGetBackground (c) != 0xABCDEF) {
	printf ("FAIL attr-bg readback: 0x%lx\n",
		(unsigned long) _XmPlatGetBackground (c)) ;
	failures++ ;
    }
    _XmPlatSetLineWidth (c, 7) ;
    if (_XmPlatGetLineWidth (c) != 7) {
	printf ("FAIL attr-width readback: %u\n", _XmPlatGetLineWidth (c)) ;
	failures++ ;
    }
    la.width = 3 ; la.style = XmPlatLineOnOffDash ; la.cap = 1 ; la.join = 2 ;
    _XmPlatSetLineAttr (c, &la) ;
    la = _XmPlatGetLineAttr (c) ;
    if (la.width != 3 || la.style != XmPlatLineOnOffDash ||
	la.cap != 1 || la.join != 2) {
	printf ("FAIL attr-lineattr readback\n") ;
	failures++ ;
    }
    _XmPlatMemCtxFree (c) ;
    printf ("attr-readback: %s\n", failures ? "FAIL" : "ok") ;
}

int
main (int argc, char **argv)
{
    (void) argc ; (void) argv ;

    test_fill_rect () ;
    test_draw_rect () ;
    test_draw_line () ;
    test_clip () ;
    test_dash () ;
    test_fill_arc () ;
    test_fill_polygon () ;
    test_segments () ;
    test_points () ;
    test_attr_readback () ;

    if (failures) {
	printf ("primtest: %d FAILURES\n", failures) ;
	return 1 ;
    }
    printf ("primtest: all prim probes passed\n") ;
    return 0 ;
}