# Motif Modernization — Migration Guide

Covers the platform-abstraction work described in `doc/plat-abstraction.md`.
Status of this guide: written after Phase 0 + Phase 1 landed (commit
`0451e00`), updated as later phases merge.  Read §1 first: it tells you
whether you need to do anything at all.

## 1. Do I need to migrate?

| You are… | Need to migrate? | When | See |
|---|---|---|---|
| An application using only public Xm API (`Xt*`, `Xm*`, `Xme*`) | **No** | never (2.x line) | §2 |
| A UIL/Mrm user (`.uil` sources, `.uid` files) | **No** | never | §2.4 |
| A custom widget written against Xm private headers (`*P.h`) | **No** (recompile only) | at next rebuild | §3 |
| A custom widget that draws directly with Xlib (`XDraw*`, `XFill*`, `XCreateGC`, `XClearArea`, `XCopy*`) | **Yes — start now** | before Phase 6 | §4 |
| A downstream fork / packager | **Yes — build system** | at next repackage | §5 |
| A vendor of an Xm fork with private-API patches | **Yes** | before Phase 6 | §6 |
| Motif window manager (mwm) config or `.mwmrc` user | **No** | never | §2.5 |

The governing rule (plan §4) is **no API break**: every signature in
`include/…/Xm.h`, `Mrm.h`, the uil grammar, the `mwm` RC file format, and the
`Xme*` functions is frozen for the 2.x line.  Phase 1 changed *how libXm
draws internally*, not what it exposes.

## 2. Applications: no source changes required

### 2.1 What stayed the same

- Every `Xm*` / `Xt*` call, every resource, every translation table works
  exactly as before.  Nothing in the public headers changed (verified: the
  Phase-1 commit touched zero installed headers outside the new `XmPlat/`
  directory).
- `XmeDrawShadows`, `XmeDrawHighlight`, `XmeDrawSeparator`, `XmeDrawArrow`,
  `XmeDrawCircle`, `XmeDrawDiamond`, `XmeDrawPolygonShadow`,
  `XmeDrawIndicator`, `XmeClearBorder` and `XmDrawBevel` keep their exact
  signatures (they still take `Display*`, `Drawable`, `GC`).  Internally
  they now forward to `XmPlat`; you call them exactly as before.
- The `libXm` soname/version-info is unchanged; drop-in binary replacement
  works for programs linked against the same 2.3.x series.

### 2.2 What will visibly change (Phase 6 only)

Rendering quality improves when Phase 6 (cairo backend) lands: ARGB visuals,
antialiased shadows, alpha icons.  Two consequences to be aware of:

1. Pixel-exact screenshots will differ.  The screenshot harness
   (`tools/gate/screenshot-harness.sh`) is the arbiter: baselines get
   re-blessed in the same commit as the rendering change.  If you diff
   libXm output pixel-for-pixel, budget for that commit.
2. Colors remain identical — the palette still flows through
   `XmSetColorCalculation`/`XmChangeColor`; no theme engine sneaks in
   behind your back.  Data-driven theming arrives as opt-in
   `XmLoadTheme` (plan §7.1), not as a behavior change.

### 2.3 Build-system notes for application developers

- `libXm` now embeds the `XmPlat` objects; you still link exactly
  `-lXm` plus the same X11/Xt/fontconfig/Xft/jpeg/png libraries as before.
- No new external dependency was added in Phases 0–1.  Phase 6 will add
  cairo (+pixman) at link time; that change will be announced in this guide.

### 2.4 UIL / Mrm users

Nothing to do.  UIL sources and compiled `.uid` files are pure data
formats; uil, wml and Mrm were untouched by Phases 0–1.

### 2.5 mwm users

Nothing to do.  `clients/mwm` was untouched by Phases 0–1.  (Plan §3
Phase 5 will put mwm's property/atom work on the same contract; `.mwmrc`
and client-programs remain stable throughout.)

## 3. Custom widgets: recompile, nothing more

If your widget includes Xm private headers (`Xm/XmP.h`, `Xm/ManagerP.h`,
…) but never calls Xlib draw primitives directly, simply recompile against
the new tree.  No private header gained an `XmPlat` dependency, so your
sources see no difference.

If you *did* poke at widget internals that moved (e.g. you called the old
inlined `XClearArea` sequences copied out of lib/Xm sources), see §4.

## 4. Custom widgets that draw directly: the real migration

**Deadline: Phase 6.**  When the cairo backend replaces the core-Xlib one,
any code still calling `XDraw*`/`XFill*`/`XCopy*`/`XCreateGC`/
`XChangeGC`/`XSetClipMask`/`XClearArea` on libXm-owned drawables will
break — those drawables will be cairo surfaces.  Migrating now is cheap
because the contract exists and is X11-shaped; migrating in Phase 6 means
rewriting against an API you can no longer test against core Xlib.

### 4.1 The 30-minute path (recommended)

Your draw code keeps its `Display*`/`Drawable`/`GC`; you route each call
through the seam helpers from `<Xm/XmPlat/XmPlatP.h>`:

```c
#include <Xm/XmPlat/XmPlatP.h>   /* internal; see note below */

/* before */
XFillRectangle (XtDisplay (w), XtWindow (w), gc, x, y, width, height);

/* after */
_XmPlatFillOneRect (XtDisplay (w), XtWindow (w), gc, x, y, width, height);
```

One-shot helpers (build ctx, draw, free):

| Xlib call | Helper |
|---|---|
| `XFillRectangle` | `_XmPlatFillOneRect (dpy, d, gc, x, y, w, h)` |
| `XDrawLine` | `_XmPlatDrawOneLine (dpy, d, gc, x1, y1, x2, y2)` |
| `XClearArea` | `_XmPlatClearOneRect (dpy, win, x, y, w, h)` |
| `XSetClipMask (dpy, gc, None)` | `_XmPlatClrClip (dpy, gc)` |

General form for anything else (lists, arcs, blits, putimage):

```c
XmPlatDrawCtx c = _XmPlatCtx (dpy, drawable, gc);   /* + drawable */
_XmPlatFillRects (c, plat_rects, n);                /* pick the prim */
_XmPlatCtxFree (c);                                 /* always pair */
```

Conversion helpers mirror the X11 structs field-for-field
(`XmPlatRect`, `XmPlatPoint`, `XmPlatSegment` — same member names, so a
loop copy is mechanical).  `_XmPlatSurfaceOf (dpy, drawable)` wraps a
pixmap/window for blit sources; `_XmPlatImageOf (XImage*)` wraps an
XImage for `_XmPlatPutImage` (Phase 3 replaces this with `XmImage`).

**Note on visibility.** `XmPlatP.h` is internal.  Custom widgets may use it
during the 2.x migration window (it is *deliberately* shipped for that
purpose), but anything you use from it must be revisited at each phase
boundary — the table above is the stable subset.  When Phase 6 lands the
one-shot helpers move to a small public convenience header; your call sites
do not change.

### 4.2 The strict path (contract-only)

If you want zero migration debt after Phase 6, go contract-pure now:

1. At widget realize/init, create long-lived handles once:

```c
XmPlatSurface surf = _XmPlatSurfaceOf (XtDisplay (w), XtWindow (w));
XmPlatDrawCtx ctx  = _XmPlatDrawCtxOf (XtDisplay (w), my_gc);
```

2. Draw only with contract primitives (`_XmPlatDraw*`, `_XmPlatFill*`,
   `_XmPlatBlit*`, `_XmPlatSet*`).
3. Attribute changes (foreground, clip, dashes) go through the setters —
   never `XChangeGC` directly.
4. Free handles in destroy.

This path costs more up front and buys you: when Phase 6 flips the backend,
your widget redraws correctly on cairo with no source change.

### 4.3 GC creation

`XCreateGC` has no contract replacement in Phase 1 by design (GCs are
X11-specific plumbing).  Use `_XmPlatCreateCtxOnSurface (surf, mask,
&values)` which returns an `XmPlatDrawCtx` and gives you the underlying GC
via `_XmPlatGcOf (ctx)` if you must stash it in a widget field.  Phase 2
moves GC management behind the contract entirely.

### 4.4 Text drawing

Phase 2 has landed, so the full font contract is available
(`lib/Xm/XmPlat/XmPlat.h` "Font contract"):

```c
/* build a token once per font (Display-carrying variants preferred) */
XmPlatFont f = _XmPlatFontOfFontStructD (dpy, fs);   /* or OfFontSetD /
                                                        OfXftFontD */

/* metrics */
int asc  = _XmPlatFontAscent (f);
int desc = _XmPlatFontDescent (f);
int w    = _XmPlatTextWidth (f, XmPlatTextMB, text, len);
XmPlatCharInfo ci;
_XmPlatTextExtents (f, XmPlatTextUTF8, text, len, &ci);

/* draw */
_XmPlatDrawString (ctx, f, XmPlatText8, text, len, x, y, 0);
_XmPlatDrawStringColored (ctx, f, kind, text, len, x, y, 0, &color);
```

Kinds: `XmPlatText8/16/MB/WC/UTF8/32` select the encoding per call; the
token itself remembers its backend font.  `_XmPlatFontOfGC` (font riding
in the GC) still works but is transitional.  The per-char XFontStruct
fields (`per_char`, `min_bounds`, `max_bounds`, `default_char`) have no
contract equivalent — string metrics prims answer what those reads
computed; custom widgets doing glyph-table math should call
`_XmPlatTextWidth`/`_XmPlatTextExtents` per character instead.

Loading a core font by name: `_XmPlatFontLoad` / `_XmPlatFontUnload`
(replaces `XLoadQueryFont`/`XFreeFont`).

### 4.5 Checklist

```
[ ] No XDraw* / XFill* / XCopy* / XPutImage / XCreateGC / XChangeGC /
    XSetClipMask / XClearArea / XClearWindow left in my widget
[ ] All clip changes via _XmPlatSetClipRect / _XmPlatSetClipMaskSurf /
    _XmPlatClrClip
[ ] Every _XmPlatCtx / _XmPlatSurface creation has a matching free
[ ] Text prims flagged for the Phase-2 font-token change (§4.4)
[ ] Widget compiles against the c89 and c99 gates (see §5.2)
```

## 5. Downstream forks and packagers

### 5.1 Build

- `lib/Xm/Makefile.in` gained hand-maintained rules for `XmPlat/*.lo`
  (automake regeneration is *not* available in this tree — see §5.3).
  If you regenerate, carry the `XMPLAT_SRCS`/`XMPLAT_HDRS` blocks from
  `lib/Xm/Makefile.am` into your regen flow; they exist so a future
  automake upgrade keeps the directory.
- `make clean` in `lib/Xm` removes `XmPlat/.libs` and `XmPlat/.deps`
  correctly; no extra dance needed for the new directory.

### 5.2 CI gates you should adopt

Run these from the tree root; both are cheap and are the contract:

```sh
tools/gate/p1-draw-gate.sh          # 0 violations required
tools/gate/screenshot-harness.sh    # renders + diffs; needs Xvfb, xwd
```

The screenshot harness runs a private Xvfb on `:97`; it is safe under
concurrent CI if you serialize (it picks its display statically — change
to `:97+$$` if you parallelize).

### 5.3 Known build quirks (unchanged from 2.3.8, restated)

- automake regeneration is unavailable (1.18 vs the old configure.ac);
  Makefile.in changes are hand-patched.  After touching configure.ac /
  Makefile.am, `touch aclocal.m4 configure config.status Makefile.in
  Makefile` instead of regenerating.
- `make clean` still deletes the shipped `tools/wml/Uil.*` and
  `clients/uil/UilLexPars.c`; restore with
  `git checkout HEAD -- tools/wml/Uil.c tools/wml/Uil.h
   tools/wml/UilLexPars.c tools/wml/UilLexPars.h clients/uil/UilLexPars.c`
  and `touch` the wml Makefiles afterwards.
- `wmlparse.c`/`wmlparse.h` must never be regenerated with bison (the
  checked-in files are byacc dialect).  If a stray build regenerated them,
  restore from git.

### 5.4 Packaging

- `libXm.so` version-info unchanged; no soname bump in Phases 0–1.
- New files to ship in the libXm package: `lib/Xm/XmPlat/*.c` compiled in,
  and nothing else.  Do **not** install `XmPlatP.h` — it is internal.
  If you ship `-devel` headers, ship `XmPlat/XmPlat.h` +
  `XmPlat/XmPlatTypes.h` only, marked as unstable (see §6).

## 6. Stability tiers (what you may depend on)

| Tier | What | Stability |
|---|---|---|
| Frozen | Public `Xm*`/`Xt*` API, `Xme*` signatures, Mrm/UIL formats, mwm RC | unchanged through 2.x |
| Stable-soon | `_XmPlat*` contract primitives in `XmPlat.h` (draw prims + setters) | stable from Phase 2; signatures will not change at Phase 6 |
| Internal | `_XmPlatCtx`/`_XmPlatSurface` constructors, one-shot helpers, `_XmPlatFontOfGC`, `_XmPlatImageOf`, `_XmPlatGcOf` | migration-window only; changes at each phase boundary |
| Volatile | `XmPlatP.h` internals (struct layouts) | never depend on these |

Rationale: the *contract* (handle types + primitive list) is what Phase 6
implements twice (core-Xlib now, cairo later); the *seam* helpers exist only
to make widget-code migration mechanical and are expected to shrink as
phases land.

## 7. Phase-by-phase expectations

| Phase | Lands | You must do | This guide |
|---|---|---|---|
| 0–1 | done (`0451e00`) | nothing (apps); §4 (custom drawing) | §4 |
| 2 — fonts | done | switch `_XmPlatFontOfGC` text sites to font tokens (`_XmPlatFontOf*D`); metrics via `_XmPlatTextWidth`/`Extents` | §4.4 |
| 3 — images | after 2 | replace `_XmPlatImageOf`/`XPutImage` sites with `XmImage` | §4 table update |
| 4 — events | after 3 | nothing yet (XEvent handling unchanged) | new section |
| 5 — atoms/DnD/mwm | after 4 | nothing for apps | new section |
| 6 — cairo backend | after 5 | custom widgets must be on the contract by now (**hard deadline**) | §4 rewrite |

## 8. FAQ

**Q: Can I keep calling `XmeDrawShadows (display, drawable, gc…)` forever?**
A: Yes — signature frozen.  It is now a forwarder; Phase 6 keeps it working
by translating inside libXm.

**Q: My widget draws into its own pixmaps with Xlib.  Phase 6 problem?**
A: Yes — pixmaps become surfaces.  Wrap them with `_XmPlatSurfaceOf` now
and draw via `_XmPlatBlit`; that code path is already cairo-ready.

**Q: Is `libXm` still drop-in binary-compatible for my app?**
A: Within 2.3.x, yes.  A rebuild is recommended (not required) to pick up
the bug fixes that rode along (DataF off-by-one, DropDown switch bug, etc).

**Q: Where is the render backend selected?**
A: Link time, inside libXm.  There is exactly one implementation at any
moment (plan §2.2 rule 1).  Applications never choose.