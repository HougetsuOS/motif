# Motif Modernization — Migration Guide

Covers the platform-abstraction work described in `doc/plat-abstraction.md`.
Status of this guide: written when the platform abstraction landed
(commit `0451e00`), updated as later work merged; the last body update
is after the theming/HiDPI/a11y work (`1d24e49`).  Read §1 first: it
tells you whether you need to do anything at all.

## 1. Do I need to migrate?

| You are… | Need to migrate? | When | See |
|---|---|---|---|
| An application using only public Xm API (`Xt*`, `Xm*`, `Xme*`) | **No** | never (2.x line) | §2 |
| A UIL/Mrm user (`.uil` sources, `.uid` files) | **No** | never | §2.4 |
| A custom widget written against Xm private headers (`*P.h`) | **No** (recompile only) | at next rebuild | §3 |
| A custom widget that draws directly with Xlib (`XDraw*`, `XFill*`, `XCreateGC`, `XClearArea`, `XCopy*`) | **Yes** | at next rebuild (the cairo backend is already the default) | §4 |
| A downstream fork / packager | **Yes — build system** | at next repackage | §5 |
| A vendor of an Xm fork with private-API patches | **Yes** | at next rebuild | §6 |
| Motif window manager (mwm) config or `.mwmrc` user | **No** | never | §2.5 |
| An app that wants theming / HiDPI / a11y | **Opt-in** | whenever you want | §4.10 |

The governing rule is **no API break**: every signature in
`include/…/Xm.h`, `Mrm.h`, the uil grammar, the `mwm` RC file format, and the
`Xme*` functions is frozen for the 2.x line.  The modernization changed
*how libXm draws internally*, not what it exposes.

## 2. Applications: no source changes required

### 2.1 What stayed the same

- Every `Xm*` / `Xt*` call, every resource, every translation table works
  exactly as before.  The installed public headers gained only additions:
  the `XmPlat/` contract headers and the new `XmLoadTheme` declaration
  (plus the `XmNscaleFactor` screen resource).
- `XmeDrawShadows`, `XmeDrawHighlight`, `XmeDrawSeparator`, `XmeDrawArrow`,
  `XmeDrawCircle`, `XmeDrawDiamond`, `XmeDrawPolygonShadow`,
  `XmeDrawIndicator`, `XmeClearBorder` and `XmDrawBevel` keep their exact
  signatures (they still take `Display*`, `Drawable`, `GC`).  Internally
  they now forward to `XmPlat`; you call them exactly as before.
- The `libXm` soname/version-info is unchanged; drop-in binary replacement
  works for programs linked against the same 2.3.x series.

### 2.2 What changed visibly (rendering)

The default build now renders through cairo (`lib/Xm/XmPlat/
XmPlatDrawCairo.c`): the same prims, a modern rasterizer.  Two
consequences to be aware of:

1. Pixel-exact screenshots may differ from older libXm builds.  The
   screenshot harness (`tools/gate/screenshot-harness.sh`) is the
   arbiter: baselines get re-blessed in the same commit as a rendering
   change.  If you diff libXm output pixel-for-pixel, budget for that.
   (With this toolchain the cairo port was byte-identical to the
   core-Xlib baseline; the fallback exists for pixel-identical
   bisects.)
2. Colors remain identical — the palette still flows through
   `XmSetColorCalculation`/`XmChangeColor`; no theme engine sneaks in
   behind your back.  Data-driven theming landed as opt-in `XmLoadTheme`
   (§4.10 below; `MOTIF_THEME` env to enable, unset changes nothing).

### 2.3 Build-system notes for application developers

- `libXm` now embeds the `XmPlat` objects; you still link exactly
  `-lXm` plus the same X11/Xt/fontconfig/Xft/jpeg/png libraries as before.
- The cairo render backend is linked into libXm (default build;
  `--disable-cairo-render` keeps the core-Xlib backend without it).
  Link line unchanged otherwise.
- `tools/gate/p7-memory-gate.sh` is a test-only artifact (never in the
  libXm dependency graph).

### 2.4 UIL / Mrm users

Nothing to do.  UIL sources and compiled `.uid` files are pure data
formats; uil, wml and Mrm were untouched by the entire modernization.

### 2.5 mwm users

Nothing to do.  `clients/mwm` now routes its atom/property work through
the same platform layer; the `.mwmrc` file format and the client
programs (`fptheme`, etc.) are stable throughout.

## 3. Custom widgets: recompile, nothing more

If your widget includes Xm private headers (`Xm/XmP.h`, `Xm/ManagerP.h`,
…) but never calls Xlib draw primitives directly, simply recompile against
the new tree.  Widget-visible private structures did not change shape
(the platform layer added fields only where noted in §4), so your
sources see no difference.

If you *did* poke at widget internals that moved (e.g. you called the old
inlined `XClearArea` sequences copied out of lib/Xm sources), see §4.

## 4. Custom widgets that draw directly: the real migration

**The cairo backend is already the default.**  Any code still calling
`XDraw*`/`XFill*`/`XCopy*`/`XCreateGC`/`XChangeGC`/`XSetClipMask`/
`XClearArea` on libXm-owned drawables bypasses the platform layer and
misses the cairo path's guarantees (and breaks if a future build drops
the core-Xlib fallback, `--disable-cairo-render`).  Migrating is cheap
because the contract exists and is X11-shaped, and the core-Xlib build
still exists to verify against.

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
pixmap/window for blit sources; `_XmPlatImageTokenOf (XImage*)` wraps
an XImage for `_XmPlatPutImage` (§4.5).

**Note on visibility.** `XmPlatP.h` is internal.  Custom widgets may use it
during the 2.x migration window (it is *deliberately* shipped for that
purpose), but anything you use from it must be revisited at each libXm
upgrade — the table above is the stable subset.  The one-shot helpers are
expected to move to a small public convenience header; your call sites
do not change when that happens.

### 4.2 The strict path (contract-only)

If you want zero migration debt against any future backend change, go
contract-pure now:

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

This path costs more up front and buys you: your widget redraws
correctly on the cairo backend with no source change, and stays correct
against any future backend.

### 4.3 GC creation

`XCreateGC` has no contract replacement by design (GCs are X11-specific
plumbing; widgets still own GCs as frozen API surface).  Use
`_XmPlatCreateCtxOnSurface (surf, mask, &values)` which returns an
`XmPlatDrawCtx`, or `_XmPlatGcOf (ctx)` to recover the underlying GC if
you must stash it in a widget field.  Both render variants treat the GC
as the attribute source, so GC-holding code keeps working on cairo.

### 4.4 Text drawing

The full font contract is available
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

### 4.5 Images

Client-side pixel buffers are `XmPlatImage` tokens
(lib/Xm/XmPlat/XmPlat.h "Image contract"):

```c
/* create / destroy */
XmPlatImage img = _XmPlatImageCreate (ctx, depth, w, h);
/* or from built-in bitmap data */
XmPlatImage bmp = _XmPlatImageBitmapOf (XtDisplay (w), bits, w, h);
/* read the screen */
XmPlatImage scr = _XmPlatImageFromSurface2 (XtDisplay (w), XtWindow (w),
                                            x, y, w, h);

/* blit */
_XmPlatPutImage (ctx, img, 0, 0, dx, dy, w, h);

/* pixel access */
unsigned long px = _XmPlatImageGetPixel (img, x, y);
_XmPlatImagePutPixel (img, x, y, px);

/* geometry/data */
_XmPlatImageWidth (img);  _XmPlatImageHeight (img);
_XmPlatImageDepth (img);  _XmPlatImageBytesPerLine (img);
unsigned char *d = _XmPlatImageData (img);

_XmPlatImageFree (img);   /* token + buffer */
```

If your widget installs images through the public `XmInstallImage`
(frozen `XImage*` signature) the token wraps the record; disown it with
`_XmPlatImageTokenFree` — never `_XmPlatImageFree` — because the cache
keeps the XImage alive.

### 4.6 Events

Handler bodies (Xt action procs, event handlers, gadget
`input_dispatch`) receive `XEvent*` as before — those signatures are
frozen — but wrap once at the top and read fields through prims:

```c
static void
MyArm (Widget w, XEvent *event, String *params, Cardinal *n)
{
  XmPlatEvent ev = _XmPlatEventOf (event);

  if (_XmPlatEventIsButtonPress (ev) &&
      _XmPlatEventButton (ev) == Button1)
    arm_at (_XmPlatEventX (ev), _XmPlatEventY (ev),
            _XmPlatEventTime (ev));
}
```

Common reads:

| You used to read | Prim |
|---|---|
| `event->xany.type` / `event->type` | `_XmPlatEventKind`, `_XmPlatEventIsType`, `_XmPlatEventIsButtonPress/Release`, `_XmPlatEventIsKeyPress/Release`, `_XmPlatEventIsMotion` |
| `.time` (any family) | `_XmPlatEventTime` |
| `.x/.y` (button/key/motion/crossing/expose) | `_XmPlatEventX` / `_XmPlatEventY` |
| `.x_root/.y_root` | `_XmPlatEventRootX` / `_XmPlatEventRootY` |
| `.button`, `.state`, `.keycode` | `_XmPlatEventButton`, `_XmPlatEventState`, `_XmPlatEventKeycode` |
| `xany.window` compare | `_XmPlatEventWindow (ev) == _XmPlatWindowOf (w)` |
| crossing `.focus/.mode/.detail` | `_XmPlatEventFocus`, `_XmPlatEventMode`, `_XmPlatEventDetail` |
| expose `.width/.height` | `_XmPlatEventWidth` / `_XmPlatEventHeight` |

Rules: never mutate the raw record (the only sanctioned writes are the
`_XmPlatEventSetX/SetY` clamp helpers); to stash an event for later
replay, hold an owning token (`_XmPlatEventCopy` / `_XmPlatEventFreeCopy`)
or the widget-record `XEvent*` you already own; pass raw `XEvent*` through
frozen signatures (callbacks, `XtCallActionProc`, `XtDispatchEvent`)
unchanged — `_XmPlatEventRaw` unwraps a token where needed.

### 4.8 Atoms and properties

`XmInternAtom` / `XmGetAtomName` keep their frozen signatures and route
through the platform backend — source compat, no action needed for
apps.

Custom widget code that interned atoms directly should switch to:

```c
Atom a = (Atom) _XmPlatInternAtomRaw (XtDisplay (w), "MY_TARGET", False);
const char *nm = _XmPlatAtomNameRaw (XtDisplay (w), a);   /* XFree it */
```

Property I/O (selection/DnD transport) uses `_XmPlatChangeProperty` /
`_XmPlatGetWindowProperty` / `_XmPlatDeleteProperty` with the same
argument shapes as the Xlib calls they replace; fabricated client
messages (WM protocols, DnD ACKs) go through `_XmPlatSendClientMessage`.
The public `Atom` type in callback structs is unchanged.

### 4.9 Checklist

```
[ ] No XDraw* / XFill* / XCopy* / XPutImage / XCreateGC / XChangeGC /
    XSetClipMask / XClearArea / XClearWindow left in my widget
[ ] All clip changes via _XmPlatSetClipRect / _XmPlatSetClipMaskSurf /
    _XmPlatClrClip
[ ] Every _XmPlatCtx / _XmPlatSurface creation has a matching free
[ ] Text prims on font tokens (§4.4)
[ ] Image buffers held as XmPlatImage tokens (§4.5)
[ ] Event handlers read XmPlatEvent prims; no `event->x*` field reads (§4.6)
[ ] Widget compiles against the c89 and c99 gates (see §5.2)
```

### 4.10 Theming, HiDPI and accessibility (opt-in, additive)

Three opt-in surfaces were added alongside the rendering work.  None of
them changes existing behavior; all are additive public API.

**Theming.**  `XmLoadTheme (shell, name)` merges a theme profile
into the shell's screen resource database and re-applies the palette
through `XmChangeColor` (derived colors stay behind
`XmSetColorCalculation`).  Theme files are ordinary Xrm resource files
looked up in `$XDG_CONFIG_HOME/motif/themes/<name>` then
`${XDG_DATA_DIRS:-/usr/share}/motif/themes/<name>`.  Three profiles ship
in the tree (`themes/`): `default`, `high-contrast`,
`monochrome-legacy`.  Applications get theming with zero code changes by
setting `MOTIF_THEME=<name>` in the environment (applied at the first
vendor-shell realize); calling `XmLoadTheme` yourself gives explicit
control.  Colors set by a theme flow through the normal color
calculation — nothing bypasses your palette code.

**HiDPI.**  `XmScreen` gained `XmNscaleFactor` (int, permille;
1000 = 1×).  Default resolution order: `MOTIF_SCALE` env, `Xft.dpi`
heuristic, 1000.  The Xft font seam scales rendition point/pixel sizes,
so text (and text-driven geometry) follows the factor for render-table
fonts.  Core-font `fontList`s are server-side bitmaps and do not scale —
move to render tables (`XmNrenderTable`, `XmRendition*`) for scalable
text.  There is no per-monitor support on the 2.x line (X11 remains the
only target platform, so the scale is per-screen).

**Accessibility.**  The bridge skeleton registers on
`XtHooksOfDisplay` and reports widget creates (name, class, role,
geometry) as JSON lines to the file named by `MOTIF_A11Y_LOG`.  Roles
derive from the class hierarchy; the planned `XmA11yRole` constraint
resource and the ATSPI D-Bus transport are follow-ups behind the same
seam.  Apps that set nothing see no behavior change. 

## 5. Downstream forks and packagers

### 5.1 Build

- `lib/Xm/Makefile.in` carries hand-maintained rules for the
  `XmPlat/*.lo`, `Theme.lo` and `XmA11y.lo` objects (automake
  regeneration is *not* available in this tree — see §5.3).  If you
  regenerate, carry the `XMPLAT_SRCS`/`XMPLAT_HDRS` blocks and the
  `Theme.c`/`XmA11y.c` entries from `lib/Xm/Makefile.am` into your
  regen flow.
- `make clean` in `lib/Xm` removes `XmPlat/.libs` and `XmPlat/.deps`
  correctly; no extra dance needed for the new directory.

### 5.2 CI gates you should adopt

Run these from the tree root; all are cheap and are the contract:

```sh
tools/gate/p1-draw-gate.sh          # no direct draw calls outside the backend
tools/gate/p2-font-gate.sh          # no direct core font calls
tools/gate/p3-image-gate.sh         # no direct XImage calls
tools/gate/p4-event-gate.sh         # no XEvent field access
tools/gate/p5-atom-gate.sh          # no direct atom/property calls (incl. mwm)
tools/gate/p7-memory-gate.sh        # headless prim verification; no X needed
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

- `libXm.so` version-info unchanged; no soname bump in the whole
  modernization.
- New files compiled into the libXm package: `lib/Xm/XmPlat/*.c`,
  `Theme.c`, `XmA11y.c`.  Do **not** install `XmPlatP.h` — it is
  internal.  If you ship `-devel` headers, ship `XmPlat/XmPlat.h` +
  `XmPlat/XmPlatTypes.h` only, marked as unstable (see §6).
- Install the theme profiles with the toolkit data
  (`<datadir>/motif/themes/…`) so `XmLoadTheme` finds them (§4.10).

## 6. Stability tiers (what you may depend on)

| Tier | What | Stability |
|---|---|---|
| Frozen | Public `Xm*`/`Xt*` API, `Xme*` signatures, Mrm/UIL formats, mwm RC | unchanged through 2.x |
| Stable-soon | `_XmPlat*` contract primitives in `XmPlat.h` (draw prims + setters) | stable; signatures held across the backend change |
| Internal | `_XmPlatCtx`/`_XmPlatSurface` constructors, one-shot helpers, `_XmPlatFontOfGC`, `_XmPlatImageTokenOf`, `_XmPlatGcOf` | migration-window only; re-check at each libXm upgrade |
| Volatile | `XmPlatP.h` internals (struct layouts) | never depend on these |

Rationale: the *contract* (handle types + primitive list) is what both
render variants implement (core-Xlib and cairo); the *seam* helpers exist
only to make widget-code migration mechanical and are expected to shrink
as the toolkit's internal migration completes.

## 7. What landed, in order (index)

All of it is already merged into the 2.x tree; the rows exist so you can
map a libXm version/changelog to the relevant section of this guide.
Detailed per-phase engineering notes live in `doc/plat-abstraction.md`
and `doc/phase*-notes.md` / `doc/beyond-notes.md`.

| Work | Status | What it means for you | This guide |
|---|---|---|---|
| Draw-primitive contract | done (`0451e00`) | apps: nothing; §4 is for custom drawing code | §4 |
| Font contract | done | switch `_XmPlatFontOfGC` text sites to font tokens (`_XmPlatFontOf*D`); metrics via `_XmPlatTextWidth`/`Extents` | §4.4 |
| Image contract | done | hold `XmPlatImage` tokens; draw via `_XmPlatPutImage`; build via `_XmPlatImageCreate`/`BitmapOf`; screen reads via `_XmPlatImageFromSurface2`; pixel access via `Get/PutPixel` prims | §4.5 |
| Event contract | done | wrap once per handler (`_XmPlatEventOf`), read `_XmPlatEvent*` prims; raw `XEvent*` stays in frozen Xt/gadget/callback signatures as opaque plumbing | §4.6 |
| Atom/property contract (+ mwm) | done | apps: nothing; `XmInternAtom`/`XmGetAtomName` are now wrappers over the contract (same signatures). lib/Xm and clients/mwm: interning via `_XmPlatInternAtomRaw`, property I/O via `_XmPlatChange/Get/DeleteProperty`, WM/DnD messages via `_XmPlatSendClientMessage` | §4.8 |
| Cairo render backend | done (default; `--disable-cairo-render` keeps core-Xlib) | apps: nothing — the render backend swapped under the frozen contract; custom drawing done against `_XmPlatDraw*` prims keeps working on both render variants | §4 |
| Headless test backend | done | apps: nothing; CI can verify the render contract without an X server (`tools/gate/p7-memory-gate.sh`) | §5.2 |
| Theming / HiDPI / a11y | done (`1d24e49`) | opt-in only: `XmLoadTheme`/`MOTIF_THEME`, `XmNscaleFactor`/`MOTIF_SCALE`, `MOTIF_A11Y_LOG`; existing apps unchanged | §4.10 |

## 8. FAQ

**Q: Can I keep calling `XmeDrawShadows (display, drawable, gc…)` forever?**
A: Yes — signature frozen.  It forwards into the platform layer, which
translates it for the active render backend.

**Q: My widget draws into its own pixmaps with Xlib.  Is that a problem
now that cairo is the default backend?**
A: Direct Xlib drawing on your own pixmaps still works (pixmaps stay
core-X objects on X11), but it bypasses the platform layer — wrap the
pixmap with `_XmPlatSurfaceOf` and draw via the prims (§4.1) so the code
stays correct on any backend.

**Q: Is `libXm` still drop-in binary-compatible for my app?**
A: Within 2.3.x, yes.  A rebuild is recommended (not required) to pick up
the bug fixes that rode along (DataF off-by-one, DropDown switch bug, etc).

**Q: Where is the render backend selected?**
A: Build time, inside libXm — cairo by default, core-Xlib behind
`--disable-cairo-render`.  There is exactly one implementation compiled
in at any moment (the plan's "never two live backends" invariant, as
amended to keep the fallback; see `doc/phase6-notes.md`).
Applications never choose.

**Q: How do I theme my app without touching its code?**
A: `MOTIF_THEME=high-contrast ./myapp` (theme file from the XDG search
path), or call `XmLoadTheme (toplevel, "high-contrast")` yourself.
Theme files are plain Xrm resource files.

**Q: Why doesn't my app scale at `MOTIF_SCALE=2000`?**
A: The scale applies to Xft-based render-table fonts.  If your widgets
use core-font `fontList`s (e.g. `fixed`), those are server-side bitmaps
with no scalable source — switch to `XmNrenderTable` with an Xft
rendition.