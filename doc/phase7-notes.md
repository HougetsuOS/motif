# Phase 7 notes — headless test backend (prim-level memory mode)

## Scope decision

The plan's Phase 7 sketches a memory-surface XmPlat implementation for
headless verification.  With Xt staying (plan guardrail), real widgets
can never run without X; the honest scope — agreed — is prim-level
verification: the render contract's vector prims and attribute setters
run against cairo image surfaces with no X server, probed pixel-by-pixel.

## Implementation

- `XmPlatP.h`: the draw-ctx token gains a memory-mode block (gc == NULL
  marks it): mirrored fg/bg rgb, line width/style/cap/join, clip rect.
  The X11 paths never read these.
- `XmPlatDrawCairo.c`:
  - `PrimState` branches on `ctx->gc == NULL`: memory ctxs draw from the
    mirror (color, line attrs, dash list) directly on the ctx's cairo
    image surface; X11 ctxs keep the GC-mirror path with its fallbacks.
  - Shared setters in `XmPlatDraw.c` grow `#ifdef XMPLAT_CAIRO_RENDER`
    gc==NULL branches: Set/GetForeground, Set/GetBackground,
    Set/GetLineWidth, Set/GetLineAttr, SetLineStyle, SetDashes (hook
    already mirror-side), SetClipRect (mirrors into ctx + cairo clip),
    Flush (no-op).  The core-Xlib build is untouched (its guards compile
    the memory branches out).
  - Memory seam: `_XmPlatMemCtxCreate/Free`, `_XmPlatMemCtxData`
    (+ `MarkDirty` for direct buffer writes), `_XmPlatMemCtxWidth/
    Height/WritePng`.
- `tests/primtest.c`: 10 subtests (fill/draw rect, line, clip on/off,
  dash on/off, fill-arc pie-slice geometry, fill-polygon, segments,
  points, attribute readback) with pixel probes; tolerance for
  antialiased edges.
- `tools/gate/p7-memory-gate.sh`: builds the test against the actual
  backend sources (separate TUs — the P.h static constructors cannot be
  included twice), links cairo/Xft/X11/Xt (no server), runs it.

## Bugs found by the prim tests (and fixed in the cairo backend)

1. **Arc angle mapping was wrong.**  The first port used the
   `x11→cairo = -angle` negation from folklore ports; empirically X11
   and cairo both measure positive angles from 3 o'clock toward +y
   (visually clockwise with y-down), so the transfer is 1:1.  The wrong
   mapping drew a 270° sweep (the "arc everywhere but where it should
   be" signature).  X11's bottom-left quadrant test caught it.
2. **Arc join mode.**  `cairo_close_path` joins the arc endpoints with a
   chord; X11's default `arc_mode` is `ArcPieSlice` (join through the
   center).  The backend now reads `GCArcMode` and joins accordingly
   (`ArcChord` → close_path; `ArcPieSlice` → line_to center).  The
   chord-only wedge left the middle unpainted, which the interior probes
   caught.
3. **Surface cache invalidation.**  Direct writes to
   `_XmPlatMemCtxData`'s buffer need `cairo_surface_mark_dirty` before
   the next cairo draw, or cairo's internal state is stale.  Exposed as
   `_XmPlatMemCtxMarkDirty`; the test's ClearToWhite calls it.

## Verification

- p7 gate: all 10 prim subtests pass, no X server involved.
- c89 from clean with `--disable-cairo-render`: 0 warnings
  (memory branches compile out; Xlib build untouched);
  screenshots byte-identical.
- c99 from clean with cairo: 0 warnings; all six gates 0; screenshots
  match.
- UBSan: periodic/hellomotif/mwm clean on the post-arc-fix build.
