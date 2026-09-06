# Phase 6 notes — modern rendering on X11 (cairo-Xlib backend)

## Survey (2026-09-06, post Phase 5b `0c047e7`)

Contract surface to re-implement:

- `lib/Xm/XmPlat/XmPlat.h`: 117 externs (draw attrs, prims, image, font,
  text, events, atoms).  Of those the *render* surface is the Phase-1 draw
  contract + Phase-3 image prims + Phase-2 text prims ≈ 70 entry points;
  event/atom prims do not touch the renderer and stay in the shared files.
- `lib/Xm/XmPlat/XmPlatDraw.c` is 2,019 lines: the whole core-Xlib
  implementation (draw + image + font/text + event + atom backends in one
  file).
- Seam constructors still in widget code (the Phase-1 migration glue the
  cairo port must keep working): `_XmPlatCtx`/`_XmPlatSurface` statics in
  `XmPlatP.h`, called from 34 files (TabBox 79, TextOut 45, DragOverS 65,
  XmString 35, DataF 26, TextF 23, Notebook 21, DrTog 17, Draw.c 13, ...).
  All funnel `(Display*, Drawable, GC)` triples through `_XmPlatCtx` —
  that is the single attribute source the cairo backend mirrors.
- Text already renders through Xft (`USE_XFT` in include/config.h; XftDraw
  cache lives in the backend, `XmRenderT.c` wrappers route via
  `_XmPlatXftDrawOf`).  Xft is Xrender-based, not cairo — the text path
  can stay Xft under the cairo backend and keep its exact appearance.
- Environment: cairo 1.18.4 (`pkg-config cairo-xlib`) available; Xrender,
  Xft, freetype already required by the tree.  ASan unavailable as before;
  UBSan tree at `/tmp/opencode/motif-asan`.

## Decisions

1. **cairo is the default render backend; core-Xlib stays selectable.**
   The plan's §2.2 rule 1 ("never two backends", cairo *replaces* core
   Xlib) is amended by agreement: cairo is the default build, and the
   core-Xlib implementation stays available behind
   `--disable-cairo-render` for pixel-identical regression bisects and
   minimal-target builds.  Only one implementation is compiled into
   libXm per build (compile-time selection, no dispatch table); the
   "never two live backends" invariant holds per binary.
2. **Selection mechanism.**  `configure` gains `--enable-cairo-render`
   (default yes).  It defines `XMPLAT_CAIRO_RENDER` in `include/config.h`
   and adds `CAIRO_CFLAGS`/`CAIRO_LIBS`.  New backend file
   `lib/Xm/XmPlat/XmPlatDrawCairo.c` (guarded `#ifdef
   XMPLAT_CAIRO_RENDER`); `XmPlatDraw.c` gets the inverse guard around the
   render sections.  Both objects can sit in the archive; exactly one
   defines each symbol.
   - Makefile regen is still broken in this tree (Phase-0 constraint), so
     `lib/Xm/Makefile.in`/`Makefile` get the same hand-patch treatment as
     the XmPlat files got in Phase 0/1 (`am__objects_13`).
3. **Attribute model.**  Widgets still own GCs (widget-record GC fields
   are frozen API surface); `_XmPlatCtx(dpy, d, gc)` keeps receiving the
   GC.  The cairo backend mirrors GC state (foreground/background pixel,
   line width/style/dash, clip rect/mask, tile/stipple, function) into
   cairo state at prim time:
   - fg/bg pixel → `XGetGCValues` + `XQueryColor` → `cairo_set_source_rgb`.
   - 1-bit stipple/tile pixmaps → cached `cairo_pattern_t` from
     `cairo_xlib_surface` + pattern (pattern cache keyed by Pixmap id).
   - clip → `cairo_region_t` / rect list from the GC clip; GC clip-mask
     pixmap → masked surface pattern.
   - `GXxor`/other GC functions: cairo has no logical-op raster; XOR
     drawing sites (drag-over visuals) keep a narrow `XSetFunction` +
     `XDraw*` fallback inside the backend (data-path exemption, same
     spirit as the XPutImage one below).  Census of GXxor sites: DrOverS,
     DragUnder — 2 files; they route through `_XmPlatBlit`/dash prims,
     whose backend can detect `GCFunction != GXcopy` from the mirrored
     GC and fall back per-prim.  No contract change.
4. **Data movement stays X11.**  `PutImage`, `Blit`, `BlitMask`,
   `ClearArea` are pixmap/scanline transport, not vector rendering: the
   cairo backend performs them with `XPutImage`/`XCopyArea` on the same
   drawable the cairo surface wraps.  This keeps the Xpm pixel engine's
   XImage pipeline lossless (its Phase-6 "collapse" is *not* in scope —
   the plan's Phase-3 exit already routed its bridge points through the
   contract; converting XImage→cairo image surfaces for identical pixel
   output buys nothing and risks the byte-exact screenshot gate).
5. **Text stays Xft** (decision 1's twin): the XftDraw machinery is
   shared, appearance-identical, and Phase 2 froze the font token
   contract; cairo text would re-rasterize every glyph for zero gain
   this phase.
6. **Offscreen double buffering, ARGB visuals, alpha shadows/gradients
   (plan §3 Phase 6 bullet):** enabled by the new backend but not forced
   through widgets this phase.  The contract gains nothing; the
   capability lives behind `XmPlatColor.alpha` (already in the token
   since Phase 1) and the backend's surface setup.  Follow-up work once
   themes (§7.1) need it.

## Implementation log

- `configure`: new `--enable-cairo-render` (default yes).  Defines
  `XMPLAT_CAIRO_RENDER` in `include/config.h`, substitutes
  `CAIRO_CFLAGS`/`CAIRO_LIBS`, appends cairo cflags to CPPFLAGS, errors
  when cairo-xlib is absent unless `--disable-cairo-render`.
  Hand-patched: `configure` (flag + probe + AC_SUBST list),
  `include/config.h.in` (template), `config/Makefile.in` (vars),
  `lib/Xm/Makefile.in` (vars + source/object + LIBADD).
- `lib/Xm/XmPlat/XmPlatDrawCairo.c` (new, ~700 lines): the 14 vector
  prims + `_XmPlatCairoCtx{Init,Fini,Dashes}` + `_XmPlatCairoClipRect`.
  Attribute model: GC stays source of truth; `PrimState` reads
  GCFunction/GCLineWidth/GCLineStyle/GCCapStyle/GCJoinStyle/GCClipMask/
  GCFillStyle per prim and mirrors into cairo.  Fallback to the core-Xlib
  call on the same drawable when GCFunction != GXcopy or a clip-mask
  pixmap is set.  Dash pattern: `_XmPlatSetDashes` records the list on
  the ctx (`_XmPlatCairoCtxDashes`) because the server-side dash list is
  not readable back.  Clip rects: `_XmPlatSetClipRect` mirrors into the
  cairo ctx (`_XmPlatCairoClipRect`) when one exists.
- `XmPlatP.h`: ctx token gains `cr` (cairo_t), dash list + offset fields;
  the static `_XmPlatCtx`/`_XmPlatCtxFree` constructors call
  Init/Fini.  cairo.h included only under XMPLAT_CAIRO_RENDER.
- `XmPlatDraw.c`: vector-prims section guarded `#ifndef
  XMPLAT_CAIRO_RENDER`; shared sections (attrs, GC readback, text, image,
  blit, clear, events, atoms) untouched; three hooks added (dashes,
  clip rect incl. the w==0 clear path, ctx-of init).
- Tiled/stippled special fills: kept on core X (see Decisions 3/4 — the
  first cairo attempt used cairo_xlib_surface_create of the tile/stipple
  pixmap; 1-bit pixmaps under a NULL visual give an errored surface, and
  the win is negligible for tab-decoration-only paths).
- Bug found at runtime: `_XmPlatCairoClipRect` originally force-created
  the cairo context.  TabBox builds clip-only ctxs with drawable 0
  (`_XmPlatCtx(dpy, 0, gc)` then `SetClipRect` + free), which made
  cairo_xlib_surface_create(dpy, 0, ...) return an error surface whose
  destroy tripped cairo's ref-count assert at redraw (periodic, TabBox
  DrawTab).  Fix: mirror only when a cairo context already exists and
  the drawable is real.  (The Xlib build was always clean — UBSan/ASan
  would not have caught this one; the screenshot + smoke runs did.)

## Verification

- c89 + c99 from clean with cairo: 0 warnings.  c89 from clean with
  `--disable-cairo-render`: 0 warnings, byte-identical screenshots.
- All five gates 0.  Screenshots (cairo build): hellomotif + periodic
  match the core-Xlib baselines byte-for-byte (cairo's rasterization of
  this widget set is pixel-identical so far — re-baselining not needed;
  one timing-flaky DIFF observed for periodic across 6 harness runs,
  matched 2/3 non-cairo runs historically).
- UBSan tree rebuilt with cairo: periodic/hellomotif/mwm 0 runtime
  errors.  mwm smoke on the cairo build: starts, parses, runs its loop.
- hellomotif "can't open hierarchy" when run from the tree root is the
  .uid lookup (CWD-dependent), not a backend issue — the harness cds
  into the app dir.