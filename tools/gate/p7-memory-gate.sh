#!/bin/sh
# Phase-7 gate: headless verification of the XmPlat render contract.
# Builds tests/primtest.c against the cairo backend sources and runs it
# with no X server (memory/image-surface draw contexts, gc == NULL).
# Prim pixel probes with tolerance; PNG artifact for humans when asked.
cd "$(dirname "$0")/../.." || exit 1

TMP=$(mktemp -d /tmp/opencode/p7.XXXXXX)
trap 'rm -rf "$TMP"' EXIT

CAIRO_CFLAGS=$(pkg-config --cflags cairo-xlib) || exit 1
CAIRO_LIBS=$(pkg-config --libs cairo-xlib) || exit 1
XFT_CFLAGS=$(pkg-config --cflags xft) || exit 1

gcc $CAIRO_CFLAGS $XFT_CFLAGS -DXMPLAT_CAIRO_RENDER -Ilib/Xm -Iinclude -I. \
    -O0 -g -std=c99 -Wall -Wextra -Wno-unused-parameter \
    -c -o "$TMP/primtest.o" tests/primtest.c 2>"$TMP/cc1.log" || {
	head -10 "$TMP/cc1.log"; echo "Phase-7 gate FAILED: primtest compile error"; exit 1
}
gcc $CAIRO_CFLAGS $XFT_CFLAGS -DXMPLAT_CAIRO_RENDER -Ilib/Xm -Iinclude -I. \
    -O0 -g -std=c99 -w -c -o "$TMP/xmplat.o" \
    lib/Xm/XmPlat/XmPlat.c 2>>"$TMP/cc2.log" || {
	head -10 "$TMP/cc2.log"; echo "Phase-7 gate FAILED: backend compile error"; exit 1
}
gcc $CAIRO_CFLAGS $XFT_CFLAGS -DXMPLAT_CAIRO_RENDER -Ilib/Xm -Iinclude -I. \
    -O0 -g -std=c99 -w -c -o "$TMP/draw.o" \
    lib/Xm/XmPlat/XmPlatDraw.c 2>>"$TMP/cc2.log" || {
	head -10 "$TMP/cc2.log"; echo "Phase-7 gate FAILED: backend compile error"; exit 1
}
gcc $CAIRO_CFLAGS $XFT_CFLAGS -DXMPLAT_CAIRO_RENDER -Ilib/Xm -Iinclude -I. \
    -O0 -g -std=c99 -w -c -o "$TMP/drawcairo.o" \
    lib/Xm/XmPlat/XmPlatDrawCairo.c 2>>"$TMP/cc2.log" || {
	head -10 "$TMP/cc2.log"; echo "Phase-7 gate FAILED: backend compile error"; exit 1
}
gcc -o "$TMP/primtest" "$TMP/primtest.o" "$TMP/xmplat.o" \
    "$TMP/draw.o" "$TMP/drawcairo.o" \
    $CAIRO_LIBS $(pkg-config --libs xft) -lX11 -lXt 2>"$TMP/cc.log" || {
	echo "Phase-7 gate FAILED: primtest build error:"
	head -10 "$TMP/cc.log"
	exit 1
}

"$TMP/primtest" || exit 1
echo "Phase-7 gate OK: headless prim verification passed"