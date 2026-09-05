# Motif Platform Abstraction Plan

Status: plan (not yet started)
Target tree: Motif 2.3.8 (`lib/Xm`, `lib/Mrm`, `clients/mwm`, `clients/uil`, `tools/wml`)
Governing constraint: **X11 is the reference platform and remains the primary —
and for the foreseeable future only — implementation target.** No Wayland, GBM,
EGL, or any other display-server backend is planned, designed, or scaffolded.
The purpose of the abstraction is to make X11 *one implementation behind a
contract*, so the rest of the toolkit stops depending on it structurally.

---

## 1. Why (measured, not guessed)

A symbol census of `lib/Xm` (210 `.c` files, ~345k LOC) gives the actual coupling
the plan must cut:

| Coupling | Scale (per §5 regexes, line counts) | Where it concentrates |
|---|---|---|
| Xlib/Xt symbols used | ~124 distinct Xlib + ~124 distinct Xt symbols | 210 files |
| `XEvent` lines | 1,811 in 87 files | TextIn (257), DataF (232), TextF (158), List (116), Container (83) |
| Core-X draw calls (`XDraw*`/`XFill*`/`XCopy*`/GC fiddling) | 806 lines in 47 files | TabBox (156), TextOut (64), DragOverS (63), TabStack (62), DataF (44), ScrollBar (42) |
| `XImage` | 197 lines in 26 files | Xpmcreate (63), TabBox (38), ImageCache (33) |
| `XFontStruct`/`XFontSet` | 128 lines in 25 files | TextOut (21), XmString (18), TextF (15), XmFontList (14), DataF (13) |
| Atoms/properties | 193 lines in 38 files | CutPaste (34), Transfer (24), DragBS (17), DragICC (14) |

There is exactly one structural asset working in our favor: Motif already
funnels most bevel/shadow/arrow/indicator drawing through the `XmeDraw*`
primitives in `lib/Xm/Draw.c`. That is a natural seam — the plan builds the
platform layer around it instead of inventing a parallel one.

The only prior art inside the tree is `lib/Xm/Xmos.c` (paths, processes,
config-file lookup). `Xmos` stays exactly as it is; `XmPlat` is its rendering/
event/window-system counterpart, not a replacement.

## 2. The contract: `lib/XmPlat`, `libXmPlat.so`

New sublibrary; headers live in `include/Xm/` alongside the generated `Xm.h`.
The contract has **no `Display *`, no `Drawable`, no `GC`, no `Window`, no
`XEvent`, no `Font`/`XFontStruct`/`XFontSet` in any signature.** Backends are
selected at link time; there is exactly one backend of each kind at any moment.

### 2.1 Handle set

| Handle | Wraps (X11 terms) | Contract |
|---|---|---|
| `XmSurface` | Window / Drawable + geometry, depth, visual class | Created, resized, destroyed by backend. Widgets get a surface handle and geometry only. |
| `XmDrawCtx` | GC + primitive dispatch | ~15 prims: `line`, `lines`, `rect`, `rects`, `arc`, `segments`, `polygon`, `fill_rect`, `fill_polygon`, `fill_arc`, `blit`, `blit_mask`, `set_clip`, `set_clip_shape`, `flush`. Plus attribute setters (foreground, background, line style, line width, stipple/tile, subwindow mode). |
| `XmFont` | FontStruct / XFontSet / Xft (unified) | Metrics (ascent/descent), per-string extents, glyph-run draw with per-run direction, exact-width drawing, list-font query. |
| `XmImage` | XImage / XPM / PNG + loader | RGBA8 buffer + optional mask, width/height, `blit` and `blit_mask` through `XmDrawCtx`, loader by content sniffing (XPM, PNG), icon-mask extraction. |
| `XmEvent` | tagged union over XEvent | `pointer` (motion/button), `key` (+text), `crossing`, `focus`, `configure`, `map`, `unmap`, `property`, `client_message`, `selection`, `dnd`, `im`, `bell`. Widgets stop reading `XEvent` fields directly. |
| `XmAtom` | interned name | Atoms become interned strings; property read/write, selection ownership, WM protocols all go through string names. |
| `XmInputContext` | XIC/XIM | IM lifecycle, filter, reset, cursor geometry — stays a backend concern entirely. |

### 2.2 Rules

1. **X11 is the only backend.** One render implementation (core Xlib now,
   cairo-Xlib in the final phase), one event source (Xt). When Phase 6 lands,
   the cairo implementation *replaces* the core-Xlib one inside the same
   backend directory — never two live implementations.
2. **No API break for users.** Public `Xm.h` signatures are frozen.
   `Xme*` functions become internal consumers of `XmPlat` (their signatures
   include `Display *`/`Drawable` and cannot change; internally they translate
   and forward). Mrm/UIL are pure data formats and are untouched.
3. **The contract is expressed in X11 vocabulary, not a lowest common
   denominator.** Clip masks, bit blits, stipple tiles, `client_message`,
   property change notifications — all real X11 concepts — are first-class in
   the contract. Nothing is added to the contract until the X11 backend needs
   it; nothing stays in the contract that X11 does not use.
4. **One-way dependency:** `lib/Xm → lib/XmPlat → X11/Xt`. Never the reverse.
   `XmPlat` must not include `Xm.h` or `X11/Intrinsic.h`.

## 3. Phases

Each phase has a greppable exit criterion — progress is a number that only goes
down. The full per-file counter command is given in §5; run it before and after
each phase and paste the numbers here as the record.

**Phase 0 — Guardrails & baseline (1–2 wk)**
- CI job that fails on: `rg 'XDraw|XFill|XCreateGC|XChangeGC|XSetClipMask|Display \*' lib/Xm -g '*.c'` — enabled at the start of Phase 1, not before (the tree fails it today, by design; the gate turns on when Phase 1 begins and only ever tightens).
- ASan/UBSan build job (the 345k LOC of C89-era code will need some fixes; budget for it).
- Screenshot harness from `demos/programs` (hellomotif, periodic, drag_and_drop) producing per-widget PNGs, diffed against committed baselines. This is the only safety net that catches rendering regressions the greps cannot.
- Establish the per-phase census baseline (§5).

**Phase 1 — Draw primitives (highest leverage, do first)**
- `XmeDrawShadows/Highlight/Arrow/Circle/Diamond/PolygonShadow/Separator/
  Indicator` (all in `lib/Xm/Draw.c`) and `DrawUtils.c` become thin wrappers:
  translate `Display*/Drawable/GC` → `XmSurface/XmDrawCtx`, then forward.
- Then migrate widgets in draw-call census order (full checklist: Appendix A):
  TabBox (156) → TextOut (64) → DragOverS (63) → TabStack (62) → DataF (44) →
  ScrollBar (42) → Notebook (33) → TextF (32) → XmString (27) → Container (26)
  → ToggleBG (22) → DrTog (20) → DragUnder (18) → ToggleB/IconG (16) →
  Obso1_2 (15) → Tree/I18List (11) → LabelG (10) → Label (9) → CascadeBG (8) →
  PushB/Outline (7) → ImageCache (7, exempt) → IconButton/CascadeB (6) →
  SpinB/DrArrow (5) → TearOff/Scale/List (4) → Sash/PanedW (3) →
  XpmCrPFrI/Xm/Region/PushBG/Paned/GeoUtils/DrHiDash/ArrowBG (2) →
  SeparatoG/ScrolledW/Frame/ArrowB (1).
- *Exit:* zero `XDraw*`/`XFill*`/`XCreateGC`/`XChangeGC` outside `lib/XmPlat`
  (ImageCache/Xpm* files exempt until Phase 3).

**Phase 2 — Text**
- Promote `USE_XFT` to the default and only path (keep core-X fallback compiled
  but not selectable, so the fallback code can be deleted at the end of the
  phase).
- `XmFontList.c`, `FontS.c`, `XmRenderT.c` re-emit through `XmFont`.
- This kills the dual FontStruct/XFontSet forks that make `TextF.c` (10.7k LOC)
  and `TextOut.c` unmaintainable.
- *Exit:* zero `XFontStruct`/`XFontSet` outside `lib/XmPlat`.

**Phase 3 — Images**
- `ReadImage.c`, `ImageCache.c`, `XpmImage.c`, `Png.c`, `Jpeg.c`, and the
  embedded `Xpm*` library files (22 files, all `XImage`-based) collapse into
  `XmImage` + backend blit.
- Kills 170 `XImage` refs; enables alpha icons.
- *Exit:* zero `XImage` outside `lib/XmPlat`.

**Phase 4 — Events (riskiest; schedule the most time here)**
- Introduce the `XmEvent` tagged union; the X11/Xt backend performs the one
  translation at dispatch time, so widgets never see `XEvent`.
- **Per-widget migration, TextF/DataF/TextIn last.** TextIn alone holds 257
  refs. The action/translation tables keep Xt dispatch (that's Xt's job and
  Xt stays); only the record the handlers read changes.
- Full checklist in Appendix A; 87 files total, order by census.
- *Exit:* zero `XEvent` field access outside `lib/XmPlat` (Xt action tables
  may keep receiving `XEvent*` as opaque pointers they hand to
  `XmPlatTranslate`).

**Phase 5 — Shells, atoms, selection, DnD, mwm**
- `XmAtom` lands; `CutPaste.c` (34), `Transfer.c` (24), `DragBS.c` (17),
  `DragICC.c` (14), `TextSel.c`/`TextFSel.c` (12 each), `VendorS.c`,
  `TxtPropCv.c`, `DragC.c`, `ColorObj.c` (CDE `_MOTIF_*` atoms) move onto the
  contract.
- `clients/mwm` (79k LOC) consumes the same layer for its window/property work.
- *Exit:* zero `XInternAtom`/`XChangeProperty`/`XGetWindowProperty`/
  `XSendEvent` outside `lib/XmPlat`.

**Phase 6 — Modern rendering *on X11***
- New implementation of the `XmDrawCtx` prims using cairo-Xlib (or XRender):
  ARGB visuals, offscreen double-buffering per toplevel, alpha shadows/gradient
  fills, SVG icons via `XmImage` loader extension.
- The Motif look upgrades without touching a single widget — every widget
  already speaks `XmDrawCtx` by this point.
- cairo implementation *replaces* the core-Xlib implementation inside
  `lib/XmPlat/x11/`; core-Xlib code is deleted, not kept as a second backend.
- *Exit:* screenshot suite passes; the tree has exactly one render
  implementation.

**Phase 7 — Test backend (optional, no display server involved)**
- A memory-surface `XmPlat` implementation (same contract, no X11 at all) for
  headless unit tests and CI screenshot diffs without a running X server.
- This is **not** a second display backend — it never creates windows, handles
  no input, and exists only to make Phases 1–5 continuously verifiable.
- Cheap once the contract exists; schedule it after Phase 1 lands if the
  screenshot harness alone proves insufficient.

## 4. Guardrails

- **No API break.** Frozen: every signature in `include/Xm/*.h`, `Mrm.h`,
  `uil` grammar, `mwm` RC file format. `Xme*` signatures are frozen too
  (they carry `Display*`/`Drawable` and cannot change); they become internal
  forwarders to `XmPlat`.
- **Never two backends of the same kind.** One event source (Xt), one render
  impl (core-Xlib, then cairo). Phase 7's memory backend is a *test* backend,
  not a display backend — it never ships in `libXm.so`'s dependency graph.
- **Migration order = census order.** The grep counter in §5 is the source of
  truth. If a widget jumps the queue, the number goes up and the gate fails.
- **Xt stays.** Replacing Xt is explicitly out of scope for this plan. Xt's
  event loop, WM protocols, resource database, timers, and EditRes are
  load-bearing; the abstraction confines it to the backend + dispatch seam.
- **Xmos stays.** `lib/Xm/Xmos.c` (paths/processes/config) is untouched.

## 5. Census commands

Paste the output into this file under the phase as the record.

```sh
# Draw calls per file (Phase 1 tracker)
rg -c 'XDraw|XFill|XCopy|XPutImage|XCreateGC|XChangeGC|XSetClipMask|XClearArea|XCopyArea' \
   lib/Xm -g '*.c' | sort -t: -k2 -rn

# XImage per file (Phase 3 tracker)
rg -c 'XImage' lib/Xm -g '*.c' | sort -t: -k2 -rn

# Font per file (Phase 2 tracker)
rg -c 'XFontStruct|XFontSet' lib/Xm -g '*.c' | sort -t: -k2 -rn

# Events per file (Phase 4 tracker) — count is raw; field access is what matters
rg -c 'XEvent' lib/Xm -g '*.c' | sort -t: -k2 -rn

# Atoms/properties (Phase 5 tracker)
rg -c 'XInternAtom|XChangeProperty|XGetWindowProperty|XSendEvent' lib/Xm -g '*.c' | sort -t: -k2 -rn

# Contract-violation scan (the Phase-0 CI gate, on at Phase 1 start)
rg -n 'XDraw|XFill|XCreateGC|XChangeGC|XSetClipMask|Display \*' lib/Xm -g '*.c' \
   -g '!lib/XmPlat/**' 2>/dev/null || true
```

## 6. Sizing

Rough effort distribution (not calendar time):

| Phase | Share of total effort | Rationale |
|---|---|---|
| 0 | 5% | Harness work |
| 1 | 25% | 806 draw-call lines across 47 files, mechanical once prim set is fixed |
| 2 | 15% | Dual font paths are the deep maintenance wound; deleting one is a win |
| 3 | 10% | Contained: Xpm/ImageCache are self-contained subsystems |
| 4 | 25% | 1,811 XEvent lines in 87 files; TextF/DataF/TextIn are 10k-line minefields |
| 5 | 12% | Atoms/selection/DnD + mwm; semantic translation is subtle |
| 6 | 5% | Pure backend work under a fixed contract |
| 7 | 3% | Memory surface once contract is stable |

## Appendix A — Per-widget / per-file migration checklist

Check items off as the file's last direct X11 call is removed. Order within a
phase is fixed by the census; do not reorder without re-running the counter
(§5) and updating this list. Counts are per-file line matches from the §5
regexes at baseline (2026-09-05, Motif 2.3.8).

### Phase 1 — draw calls (target: 0 outside XmPlat; Xpm*/ImageCache exempt)

Total at baseline: 806 lines in 47 files.

```
[ ] lib/Xm/Draw.c + DrawUtils.c   (prim wrappers land here first)
[ ] lib/Xm/TabBox.c        156    [ ] lib/Xm/ToggleB.c       16
[ ] lib/Xm/TextOut.c        64    [ ] lib/Xm/IconG.c         16
[ ] lib/Xm/DragOverS.c      63    [ ] lib/Xm/Obso1_2.c       15
[ ] lib/Xm/TabStack.c       62    [ ] lib/Xm/Tree.c          11
[ ] lib/Xm/DataF.c          44    [ ] lib/Xm/I18List.c       11
[ ] lib/Xm/ScrollBar.c      42    [ ] lib/Xm/LabelG.c        10
[ ] lib/Xm/Notebook.c       33    [ ] lib/Xm/Label.c          9
[ ] lib/Xm/TextF.c          32    [ ] lib/Xm/CascadeBG.c      8
[ ] lib/Xm/XmString.c       27    [ ] lib/Xm/PushB.c          7
[ ] lib/Xm/Container.c      26    [ ] lib/Xm/Outline.c        7
[ ] lib/Xm/ToggleBG.c       22    [ ] lib/Xm/ImageCache.c     7*
[ ] lib/Xm/DrTog.c          20    [ ] lib/Xm/IconButton.c     6
[ ] lib/Xm/DragUnder.c      18    [ ] lib/Xm/CascadeB.c       6
[ ] lib/Xm/Draw.c           17    [ ] lib/Xm/SpinB.c          5
                                  [ ] lib/Xm/DrArrow.c        5
[ ] lib/Xm/TearOff.c         4    [ ] lib/Xm/Scale.c          4
[ ] lib/Xm/List.c            4    [ ] lib/Xm/Sash.c           3
[ ] lib/Xm/PanedW.c          3    [ ] lib/Xm/DrawUtils.c      3
[ ] lib/Xm/XpmCrPFrI.c       2*   [ ] lib/Xm/Xm.c             2
[ ] lib/Xm/Region.c          2    [ ] lib/Xm/PushBG.c         2
[ ] lib/Xm/Paned.c           2    [ ] lib/Xm/GeoUtils.c       2
[ ] lib/Xm/DrHiDash.c        2    [ ] lib/Xm/ArrowBG.c        2
[ ] lib/Xm/SeparatoG.c       1    [ ] lib/Xm/ScrolledW.c      1
[ ] lib/Xm/Frame.c           1    [ ] lib/Xm/ArrowB.c         1
```
`*` Xpm-family and ImageCache files stay exempt until Phase 3.

### Phase 2 — fonts (XFontStruct / XFontSet)

Total at baseline: 128 lines in 25 files.

```
[ ] lib/Xm/TextOut.c      21     [ ] lib/Xm/List.c           3
[ ] lib/Xm/XmString.c     18     [ ] lib/Xm/I18List.c        3
[ ] lib/Xm/TextF.c        15     [ ] lib/Xm/LabelG.c         2
[ ] lib/Xm/XmFontList.c   14     [ ] lib/Xm/IconG.c          2
[ ] lib/Xm/DataF.c        13     [ ] lib/Xm/Xm.c             1
[ ] lib/Xm/TabBox.c        8     [ ] lib/Xm/ToggleBG.c       1
[ ] lib/Xm/XmIm.c          7     [ ] lib/Xm/ToggleB.c        1
[ ] lib/Xm/XmRenderT.c     4     [ ] lib/Xm/TextIn.c         1
[ ] lib/Xm/Text.c          4     [ ] lib/Xm/PushB.c          1
[ ] lib/Xm/Screen.c        3     [ ] lib/Xm/ObsoXme.c        1
                                  [ ] lib/Xm/Label.c          1
                                  [ ] lib/Xm/IconButton.c     1
                                  [ ] lib/Xm/FontS.c          1
                                  [ ] lib/Xm/CascadeBG.c      1
                                  [ ] lib/Xm/CascadeB.c       1
```

### Phase 3 — images (XImage)

Total at baseline: 197 lines in 26 files.

```
[ ] lib/Xm/Xpmcreate.c    63     [ ] lib/Xm/XpmWrFFrP.c      2
[ ] lib/Xm/TabBox.c       38     [ ] lib/Xm/XpmWrFFrI.c      2
[ ] lib/Xm/ImageCache.c   33     [ ] lib/Xm/XpmCrPFrI.c      2
[ ] lib/Xm/Xpmscan.c      16     [ ] lib/Xm/XpmCrIFrP.c      2
[ ] lib/Xm/MessageB.c      4     [ ] lib/Xm/XpmCrIFrDat.c    2
[ ] lib/Xm/DragIcon.c      4     [ ] lib/Xm/XpmCrDatFrP.c    2
[ ] lib/Xm/XpmRdFToI.c     3     [ ] lib/Xm/XpmCrDatFrI.c    2
[ ] lib/Xm/XpmCrIFrBuf.c   3     [ ] lib/Xm/XpmCrBufFrP.c    2
[ ] lib/Xm/Png.c           3     [ ] lib/Xm/XpmCrBufFrI.c    2
[ ] lib/Xm/DataF.c         3     [ ] lib/Xm/ReadImage.c      2
                                  [ ] lib/Xm/Obso2_0.c        2
[ ] lib/Xm/XpmRdFToP.c     1     [ ] lib/Xm/XpmCrPFrDat.c    1
[ ] lib/Xm/XpmCrPFrBuf.c   1     [ ] lib/Xm/Region.c         1
[ ] lib/Xm/Jpeg.c          1
```
(The Xpm* support files — XpmAttrib/Xpmdata/Xpmhashtab/Xpmmisc/Xpmparse/
Xpmrgb/Xpms_popen — collapse together with Xpmcreate.)

### Phase 4 — events (XEvent; raw line counts, field access is the real metric)

Total at baseline: 1,811 lines in 87 files. Top 38 below; migrate the long
tail (49 files, ≤10 lines each) in census order as they come up. TextIn,
DataF, TextF deliberately last.

```
[ ] lib/Xm/TextIn.c       257    [ ] lib/Xm/CascadeBG.c      24
[ ] lib/Xm/DataF.c        232    [ ] lib/Xm/ArrowBG.c        21
[ ] lib/Xm/TextF.c        158    [ ] lib/Xm/ArrowB.c         20
[ ] lib/Xm/List.c         116    [ ] lib/Xm/RowColumn.c      19
[ ] lib/Xm/Container.c     83    [ ] lib/Xm/RCPopup.c        19
[ ] lib/Xm/TabBox.c        49    [ ] lib/Xm/ScrolledW.c      18
[ ] lib/Xm/RCMenu.c        46    [ ] lib/Xm/DropDown.c       17
[ ] lib/Xm/SpinB.c         38    [ ] lib/Xm/Primitive.c      15
[ ] lib/Xm/ScrollBar.c     36    [ ] lib/Xm/CutPaste.c       14
[ ] lib/Xm/PushBG.c        34    [ ] lib/Xm/MenuUtil.c       13
[ ] lib/Xm/CascadeB.c      34    [ ] lib/Xm/IconButton.c     12
[ ] lib/Xm/ToggleBG.c      33    [ ] lib/Xm/I18List.c        12
[ ] lib/Xm/PushB.c         32    [ ] lib/Xm/Transfer.c       10
[ ] lib/Xm/ToggleB.c       31    [ ] lib/Xm/TextOut.c        10
[ ] lib/Xm/ComboBox.c      31    [ ] lib/Xm/TabStack.c       10
[ ] lib/Xm/MenuShell.c     28    [ ] lib/Xm/PrintS.c         10
[ ] lib/Xm/Manager.c       27    [ ] lib/Xm/Label.c          10
[ ] lib/Xm/DrawnB.c        26    [ ] lib/Xm/GrabShell.c      10
[ ] lib/Xm/TravAct.c       25    [ ] lib/Xm/DragC.c          25
```

### Phase 5 — atoms / properties

Total at baseline: 193 lines in 38 files.

```
[ ] lib/Xm/CutPaste.c     34     [ ] lib/Xm/DropTrans.c      3
[ ] lib/Xm/Transfer.c     24     [ ] lib/Xm/VendorSE.c       2
[ ] lib/Xm/DragBS.c       17     [ ] lib/Xm/ValTime.c        2
[ ] lib/Xm/DragICC.c      14     [ ] lib/Xm/List.c           2
[ ] lib/Xm/TextSel.c      12     [ ] lib/Xm/IsMwmRun.c       2
[ ] lib/Xm/TextFSel.c     12     [ ] lib/Xm/I18List.c        2
[ ] lib/Xm/Text.c          8     [ ] lib/Xm/DataF.c          2
[ ] lib/Xm/VirtKeys.c      5     [ ] lib/Xm/AtomMgr.c        2
[ ] lib/Xm/DragC.c         5     [ ] lib/Xm/Container.c      5
[ ] lib/Xm/TextIn.c        4     [ ] lib/Xm/ColorObj.c       4
[ ] lib/Xm/TextF.c         3     [ ] lib/Xm/TearOff.c        3
[ ] lib/Xm/PrintS.c        3     [ ] lib/Xm/Label.c          3
[ ] lib/Xm/XmString.c      1     [ ] lib/Xm/Xm.c             1
[ ] lib/Xm/SelectioB.c     1     [ ] lib/Xm/Screen.c         1
[ ] lib/Xm/Scale.c         1     [ ] lib/Xm/ResEncod.c       1
[ ] lib/Xm/ResConvert.c    1     [ ] lib/Xm/Protocols.c      1
[ ] lib/Xm/FileSB.c        1     [ ] lib/Xm/EditresCom.c     1
[ ] lib/Xm/Display.c       1     [ ] lib/Xm/DataFSel.c       1
[ ] clients/mwm  (property/WM-protocol work moves onto the contract)
```

### Census record

(append dated counter output here per phase)

```
# baseline — 2026-09-05, Motif 2.3.8 tree, §5 regexes, raw line-match counts
draw:   806 lines / 47 files      image:  197 lines / 26 files
font:   128 lines / 25 files      event: 1811 lines / 87 files
atoms:  193 lines / 38 files

# after Phase 1 — 2026-09-06
draw:     0 lines / 0 files (widget code; 37 lines in XmPlat/XmPlatDraw.c
          = the backend implementation, which is where they belong)
image:  197 lines / 26 files (unchanged — Phase 3)
font:   128 lines / 25 files (unchanged — Phase 2)
event:  1811 lines / 87 files (unchanged — Phase 4)
atoms:  193 lines / 38 files (unchanged — Phase 5)
```

Phase-1 exit criterion met: zero `XDraw*`/`XFill*`/`XCreateGC`/`XChangeGC`/
`XSetClipMask`/`XClearArea` outside `lib/Xm/XmPlat` (Xpm*/ImageCache exempt
until Phase 3).  Gate: `tools/gate/p1-draw-gate.sh`.  Screenshot baselines:
`tests/screenshots/` (hellomotif, periodic) — harness `tools/gate/screenshot-harness.sh`.

X11 surface/ctx handles cross into widget code at the seam via stack
constructors (`_XmPlatCtx`/`_XmPlatSurface`, `XmPlatP.h`); when Phase 2/6
rework GC/font management these constructors change in one place.  Text
drawing temporarily rides the GC's font (`_XmPlatFontOfGC`) until the Phase-2
`XmFont` contract replaces it.

---

## 7. Beyond the abstraction: modernizing the toolkit surface

The phases above modernize *how pixels reach the screen*. They do not change
*what the toolkit looks like* or *who can use it*. Three workstreams extend
the plan to close that gap. Each one lands after the phase it depends on;
none requires an API break beyond new resources and new functions (additive,
like every Xm release before).

### 7.1 Theming — data-driven appearance (after Phase 6)

**Problem.** Colors are computed, not loaded: `XmScreen` carries a
`color_calc_proc` (`lib/Xm/ScreenP.h:89`) and `XmSetColorCalculation`
(`lib/Xm/Color.c:1053`) derives foreground/shadow/select colors from the
background pixel at init time. `XmChangeColor` (`lib/Xm/ChColor.c:47`)
recalculates a whole color set from one Pixel. Fonts are per-widget
`XmNfontList`/`XmNrenderTable` resources; every app re-specifies them.
The look is welded into C: bevel style, shadow thickness defaults,
highlight on/off, spacing constants in class records.

**Approach — theme as resource layer, not a new engine.** Motif already has
the right seams; they need to be promoted and fed from a file:

1. **Theme profile = an Xt resource file by another name.** `XtRString` →
   converted resources can already drive every widget (`*foreground`,
   `*fontList`, `*shadowThickness`). Ship `XmLoadTheme(Widget shell,
   const char *name)`: reads `${XDG_CONFIG_HOME}/motif/themes/<name>` plus
   `${datadir}/motif/themes/<name>` (XDG Base Directory spec), merges into
   the per-screen resource DB via `XrmCombineFileDatabase`, then
   `XtSetValues` the shell tree. No new state model — the existing
   resource database *is* the theme store.
2. **Color-scheme key.** A theme file names a palette
   (background/foreground/active/shadow pairs). On load, the palette is
   installed through the existing `XmSetColorCalculation` hook so derived
   colors (shadows, insensitive stipple) still come from one calculation
   point. `XmSetColorMapping`-style pixel remapping keeps 8-bit visuals
   from thrashing the colormap.
3. **Fonts via RenderTable strings.** With Phase 2 done (Xft-only), a theme
   line `*fontList: Noto Sans-10` maps to an Xft pattern directly. Kill the
   per-app font boilerplate in one file.
4. **What must *not* be re-themed via file:** geometry constants and
   traversal behavior stay class-record defaults; themes set *resources*
   only. This keeps theme files declarative and safe.

**Exit criterion:** `hellomotif` renders in three committed theme files
(default / high-contrast / monochrome-legacy) with zero app-code changes;
screenshot harness diffs all three.

**Non-goals:** CSS selectors, runtime theme switching mid-interaction (switch
at shell-realize time only), gradient/pixmap themes.

### 7.2 HiDPI — scale factor (after Phase 2, before Phase 6)

**Problem.** Motif positions in pixels; `XmScreen` exposes
`horizontalFontUnit`/`verticalFontUnit` (`lib/Xm/Screen.c:157-164`, derived
from `HeightMMOfScreen`) and `XmNunitType` (`Xm100TH_MILLIMETERS`,
`lib/Xm/Xm.h:322`) exist but defaults are `XmPIXELS`. On a 2× display the
toolkit draws 1× -sized UI.

**Approach:**

1. **One scale source.** New `XmScreen` resource `XmNscaleFactor`
   (int, ‰; default from `Xft.dpi`/96 heuristic, overridable by env
   `MOTIF_SCALE` and resource). All font metrics flow through `XmFont`
   (Phase 2 contract) already — multiply at that seam. Widget geometry:
   multiply `Primitive`/`Manager` unit conversion (the `unit_type`
   machinery already centralizes this) instead of every geometry routine.
2. **Xft already DPI-aware** (`FontS.c:724` computes yres from the screen);
   Phase 2 makes that the only path, so fonts scale by asking Xft for
   pixel size = point size × scale × yres — no bitmap-font hackery.
3. **Icons:** `XmImage` (Phase 3) carries width/height in the handle;
   blit-time scaling (nearest-neighbor first; cairo `image_scale` once
   Phase 6 lands) for `XmNlabelPixmap`.
4. **No per-monitor support in 2.x line** — scale is per-screen, set at
   `XmScreen` init. Per-monitor is a Wayland-era problem and explicitly
   out of scope (§ governing constraint).

**Exit criterion:** `periodic` and `hellomotif` at `MOTIF_SCALE=200` are
pixel-diffable against 1× reference × 2 upscale within a tolerance band;
no hardcoded-pixel regressions (audit greps `XmPIXELS` defaults).

### 7.3 Accessibility — first, not last (starts Phase 1)

**Problem.** Zero a11y surface today (no ATK/ATSPI in the tree). Widgets
convey state only via X properties a screen reader cannot interpret. Doing
a11y "later" forces a full re-audit; doing it alongside Phase 1/4 costs
incrementally.

**Approach — bridge, not toolkit:**

1. **`lib/XmA11y` (new, optional, dlopen-able)** implementing a small
   interface: name, role, description, state set, value, caret, selection,
   text accessors, action invoker. Widgets register through a
   `XmA11yDescribeProc` added as a *constraint* resource
   (`XmA11yRole`, `XmA11yName` defaulting to `XmNlabelString`) — additive,
   defaults preserve binary compat.
2. **Bridge to ATSPI over D-Bus** (the standard Linux path), implemented in
   the optional library only; `libXm` links it at runtime, never hard.
3. **Keyboard is already Motif's strength** — traversal, mnemonics,
   accelerators all exist. The bridge mainly exposes it: state reporting,
   caret tracking for TextF/DataF (Phase 4 makes caret geometry available
   at the `XmEvent` seam — expose it there).
4. **Hook point:** `XtHooksOfDisplay` (`Intrinsic.h:2237`) already fires on
   widget create/destroy — use it to register/unregister ATSPI objects
   without touching widget instantiation paths.

**Exit criterion:** with the bridge loaded, `hellomotif`'s push button and
text field are visible in `accerciser`/`at-spi-inspector` with correct
role/name/state; text caret movement is reported.

### 7.4 Sequencing note

```
Phase 1 ────────────────► 7.3 a11y bridge skeleton (describe-proc + hooks)
Phase 2 ────────────────► 7.2 HiDPI (font seam is ready)
Phase 2 + 3 ────────────► 7.1 theming (RenderTable strings + XmImage)
Phase 6 ────────────────► 7.2/7.1 polish (cairo scaling, alpha themes)
```

Sizing within the "beyond" workstream: 7.1 ≈ 8%, 7.2 ≈ 5%, 7.3 ≈ 10% of
total project effort; 7.3 is the only one that must *start* early (Phase 1)
even though it *finishes* last, because the describe-proc is cheapest to
add while class records are being touched anyway.