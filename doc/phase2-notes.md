# Phase 2 (Text/Fonts) — Implementation Notes

Work log for Phase 2 of the platform abstraction (doc/plat-abstraction.md §3
Phase 2). This file accumulates the survey, design decisions and progress;
final numbers go into the plan's census record.

## Survey (2026-09-06, tree at commit 436a971)

### What is already Xft-first

`USE_XFT` defaults to 1 (`lib/Xm/Xm.h:49`, `include/config.h:368`) and no
build in this tree turns it off.  The Xft path is the *primary* text path
already; core-X (`XFontStruct`/`XFontSet`) paths are the compat fallbacks.
Phase 2 therefore does **not** "introduce Xft" — it cleans up the contract
around what is already the default.

### The three font representations in flight

| Type | Lives in | Drawn by | Metrics via |
|---|---|---|---|
| `XFontStruct` (8/16-bit) | rendition `font` when `XmFONT_IS_FONT`, `data->font` when `!use_fontset` | `_XmPlatDrawString` (kind 8/16) via GC font token | `XTextWidth`, `XTextExtents(16)`, direct `per_char`/`min_bounds`/`max_bounds` reads |
| `XFontSet` (MB/WC/UTF8) | rendition `font` when `XmFONT_IS_FONTSET`, `data->font` when `use_fontset`, XIM (`XmIm.c`) | `_XmPlatDrawString` (kind MB/WC), `XmbDrawString`/`XwcDrawString`/`Xutf8DrawString` leftovers | `XmbTextEscapement`, `XwcTextEscapement`, `XmbTextExtents`, `XFontsOfFontSet` |
| `XftFont` | rendition `xftFont` when `XmFONT_IS_XFT`, `data->font` when `use_xft` (union-cast through the same field!) | `_XmXftDrawString`/`_XmXftDrawString2` (XmRenderT.c helpers, create an XftDraw per window via a static cache, read GC foreground → XQueryColor → XftColor) | `XftTextExtentsUtf8/16/32`, direct `font->ascent/descent/max_advance_width` |

**Landmine (already present, we inherit it):** `OutputDataRec.font`
(TextOutP.h:121) and `XmTextRec.text.font` (TextFP.h:106 under `USE_XFT`)
are `XFontStruct*`-typed but actually hold an `XftFont*` whenever
`use_xft` is set; every reader must check `use_xft`/`use_fontset` before
touching it.  Same pattern in DataF (`XmTextF_font` vs
`XmTextF_xft_font` — same field, two accessor macros with different
casts).

### Call-site census (Phase-2 exit metric)

Raw calls/metrics reads outside `lib/Xm/XmPlat/` (grep categories, 2026-09-06):

```
draw-string:   XmbDrawString 10, XwcDrawString 4, Xutf8DrawString 1,
               XmbDrawImageString 2, XwcDrawImageString 2,
               XDrawString 3, XDrawString16 2, XDrawImageString 2,
               XDrawImageString16 2          (= 28 sites)
extents:       XTextWidth 17, XTextExtents 12, XmbTextEscapement 14,
               XmbTextExtents 5, XftTextExtentsUtf8 16,
               XftTextExtents16 3, XftTextExtents32 1,
               XwcTextEscapement (TextF) 2   (= ~70 sites)
metrics:       direct font-struct field reads (per_char, min_bounds,
               max_bounds, min_char_or_byte2, default_char, ascent,
               descent): TextOut 46, TextF 6, DataF 6, TabBox 5,
               I18List 2, List 1                        (= 66 sites)
load/free:     XLoadQueryFont 6, XFreeFont 5, XFontsOfFontSet 3,
               XListFonts/XFreeFontNames (FontS.c) 2    (= 16 sites)
GC font state: XGetGCValues(GCFont) 1 (XmString.c:4032), XSetFont 0
Xft plumbing:  XftDrawCreate 11, XftDrawDestroy 3, XftDrawSetClip 4,
               XftDrawString* 6, XftDrawRect 2, XftFontMatch 1,
               XftFontOpenPattern 2                     (= 29 sites)
selection:     XmbTextListToTextProperty 38             (Phase 5 territory,
               counted but NOT targeted by Phase 2)
XIM input:     XmbLookupString/XmbResetIC 5 (XmIm.c)    (stays — XIM stays)
```

Files by total (call+metric sites): XmRenderT 65, XmString 32, TextOut 26,
TextF 19, DataF 16, FontS 5, ToggleBG 3, TextIn 3, XmFontList 2, ToggleB 2,
Text 2, LabelG 2, Label 2, TabBox 1, CascadeBG 1 — plus ~12 files with
typedef-only references (zero real calls; they just declare
`XFontStruct *` fields).

### Where the seams already exist (Phase-1 leftovers we now finish)

1. `_XmPlatDrawString(ctx, font, kind, ...)` — raw text prim, token =
   `_XmPlatFontOfGC` (GCFont) or `_XmPlatFontOfFontStruct/FontSet`.
   XmString.c/TextOut.c/TextF.c/DataF.c/Scale.c already call it for the
   8/16-bit paths; the MB/WC/UTF8 branches still call `XmbDrawString` etc.
2. `_XmXftDrawString(2)` in XmRenderT.c: the Xft draw path never went
   through XmPlat at all (Phase 1 scoped draw prims to GC-mediated ops;
   Xft is a separate code path with its own XftDraw cache).
3. Widget `GC`-stashing is fine to keep (GC is a draw-ctx concern, P1
   seam `_XmPlatGcOf` keeps it working); font *loading/freeing* is what
   Phase 2 moves behind the contract.

## Design decisions

### D1. XmPlatFont becomes the one font handle (kinds 8/16/MB/WC/XFT)

`struct _XmPlatFontRec { void *f; Font fid; int kind; }` gains
`XmPlatTextXFT = 5` (holds `XftFont*`; may also carry the `XftDraw`
hint via the draw ctx's surface window).  Existing kinds stay.

Constructors (seam, XmPlatP.h — internal):
- `_XmPlatFontOfFontStruct (XFontStruct *)` — exists
- `_XmPlatFontOfFontSet (XFontSet)` — exists
- `_XmPlatFontOfXftFont (XftFont *)` — NEW
- `_XmPlatFontOfGC (Display*, GC)` — exists (GCFont token; 8/16 only)

### D2. Text prims move from "GC font" to explicit font token

`_XmPlatDrawString` keeps its signature but the `XmPlatTextGC` kind
becomes a transitional shim only (kept because XmString.c's 8/16
branches rely on it; once every draw site passes a real token the GC
kind is used only by the fallback converter in ResConvert).

New prim (UTF8):
- `_XmPlatDrawStringUtf8 (ctx, font, text, len, x, y, image)` —
  backend: `Xutf8DrawString`/`Xutf8DrawImageString` for FontSet fonts,
  `XftDrawStringUtf8` for Xft fonts.  (Removes the last
  `Xutf8DrawString` outside the backend.)

### D3. Metrics prims replace field reads and XTextWidth/Extents calls

The metrics that TextOut/TextF/DataF/TabBox/List/I18List actually
consume reduce to six primitives:

```c
/* line metrics */
extern int _XmPlatFontAscent  (XmPlatFont f);
extern int _XmPlatFontDescent (XmPlatFont f);
extern int _XmPlatFontHeight  (XmPlatFont f);

/* string metrics (returns advance width in pixels) */
extern int _XmPlatTextWidth (XmPlatFont f, int kind,
                             const void *text, int len);
extern int _XmPlatTextWidthUtf8 (XmPlatFont f, const char *text, int len);

/* full XCharStruct-equivalent for the one site that needs lbearing/
   rbearing (TextOut _FontStructFindExtent); XmPlatCharInfo mirrors
   XCharStruct field-for-field */
typedef struct { short lbearing, rbearing, width;
                 short ascent, descent; unsigned short attributes; } XmPlatCharInfo;
extern void _XmPlatTextExtents (XmPlatFont f, int kind,
                                const void *text, int len,
                                XmPlatCharInfo *overall);
```

Backend mapping:
- FontStruct kind → `XTextWidth`/`XTextExtents`/direct fields (same
  behavior as today, one file).
- FontSet kind → `XmbTextEscapement`/`XmbTextExtents` (first font of
  the set for line metrics, `XFontsOfFontSet()[0]`).
- Xft kind → `XftTextExtentsUtf8/8/16`, `font->ascent/descent/height`,
  average width via the "0" glyph / `_XmXftFontAverageWidth` logic.

Consequence: `per_char`/`min_char_or_byte2`/`default_char` lookups in
TextOut collapse into `_XmPlatTextWidth`/`_XmPlatTextExtents` (the
backend decides per_char vs min_bounds vs default_char).  This kills
the deep coupling without an Xft per-glyph metrics API (Xft's
`XftGlyphExtents` remains a backend-internal option later if cairo
needs it).

### D4. XftDraw plumbing moves behind the contract

`_XmXftDrawCreate/Destroy` (static per-window cache in XmRenderT.c)
becomes XmPlat-internal.  New prims:

```c
/* backend: no-op for core fonts; XftDrawCreate for Xft kind */
extern void _XmPlatDrawStringXft (XmPlatDrawCtx ctx, XmPlatFont font,
                                  int bpc, const void *text, int len,
                                  int x, int y,
                                  const XmPlatPixel *fg);
```

However — simpler and sufficient: `_XmPlatDrawString` with an
`XmPlatTextXFT`-kind font token handles fg color by reading the ctx's
GC foreground exactly as `_XmXftDrawString2` does today (pixel →
XQueryColor → XftColor).  No separate prim needed; the XftDraw cache
moves verbatim into XmPlatDraw.c.  `_XmXftDrawString` (the rendition
variant with image/bg/underline handling) stays in XmRenderT.c but
calls `_XmPlatDrawString`-equivalent internals; its bg-rect becomes
`_XmPlatFillRect`.

`_XmXftSetClipRectangles` (Label/LabelG/XmString/List call sites)
maps to the existing `_XmPlatSetClipRect` on a ctx (XftDrawSetClip is
subsumed: the Xft backend's draw-cache clip updates alongside GC clip).

### D5. Loading/freeing fonts moves behind the contract

- `XLoadQueryFont` sites (Scale.c 4, FontS.c 2): Scale's "load default
  font" fallback becomes `_XmPlatFontLoad (Display*, const char *name)`
  returning `XmPlatFont` + `_XmPlatFontFree`.  FontS.c (XmFontSelector)
  keeps XListFonts for now — it is a font *browser* UI, its whole job
  is enumerating X fonts; it gets a pass this phase (documented), and
  migrates when the default-font story settles.
- `XFontsOfFontSet` sites (XmFontList.c `_XmGetFirstFont`, XmRenderT.c
  extent computation): `_XmPlatFontAscent/Descent/Height` on a
  FontSet-kind token answers what those sites actually want.  The
  `XmeRenderTableGetDefaultFont` public API (returns `XFontStruct*`,
  frozen) keeps working via the backend's FontSet→first-FontStruct
  path; internally renditions carry `XmPlatFont`.
- `_XmIsISO10646` (Xm.c): takes `XFontStruct*` in its *public-ish*
  internal signature; it stays but only XmPlat internals may call it
  after this phase (it moves next to the backend in P2b if trivial).

### D6. What deliberately does NOT change in Phase 2

- `XmRenderTable`/`XmRendition` resource API (`XmNfont`, `XmNfontName`,
  `XmNxftFont`, `XmRenditionRetrieve`) — public API, frozen.  Renditions
  keep `fontType` and store the loaded font internally as `XmPlatFont`
  (the `XtPointer font` field's payload becomes the token for new code;
  both old casts keep working because the token layout is not exposed).
- `XmeRenderTableGetDefaultFont` signature (frozen, returns XFontStruct*).
- `XmFontListEntryCreate/GetFont` semantics (frozen; they traffic in
  `XFontStruct*`/`XFontSet` void*).
- XIM (`XmIm.c`): `XFontSet` stays for XIC input context (X11-only API,
  no contract value; revisited at Phase 5).
- `XmbTextListToTextProperty` & friends: selection/COMPOUND_TEXT
  machinery = Phase 5.
- Screen.c's `XmNfont` resource + `GetUnitFromFont`: consumes
  `XGetFontProperty`/min_bounds/max_bounds for font units.  Ported to
  `_XmPlatTextExtents` + a new `_XmPlatFontGetQuadWidth`-style prim
  only if cheap; otherwise XFontStruct stays here this phase (1 site,
  documented).

## Execution order (census order within the phase)

1. **Contract + backend**: extend XmPlatTypes.h (kind XFT, CharInfo),
   XmPlat.h (metrics prims, UTF8 prim), XmPlatP.h (font-of-Xft
   constructor), XmPlatDraw.c (metrics impls, Xft draw cache + Xft
   string prims).  Build clean at c89.
2. **XmRenderT.c** (65 sites): route `_XmXftDrawString(2)`,
   `_XmXftSetClipRectangles`, `_XmXftFontAverageWidth` internals
   through the contract; rendition font access via `_XmPlatFontOf*`.
3. **XmString.c** (32): replace remaining Xmb/Xwc/Xutf8 draw calls
   with `_XmPlatDrawString` (+Utf8); replace XTextWidth/Extents with
   prims; XGetGCValues font site.
4. **TextOut.c** (26 + 46 metric reads): FindWidth/FindExtent helpers
   onto `_XmPlatTextWidth/Extents`; init block onto ascent/descent/
   max-advance prims; tabwidth via TextWidth prim.
5. **TextF.c / DataF.c** (19/16): FindPixelLength, XTextWidth sites,
   image_gc GCFont read, mb/wc draw branch.
6. **The small ones** (ToggleB/G, Label/G, TabBox, List, I18List,
   Text, TextIn, Screen, Xm.c): metric reads + typedefs per D3/D6;
   typedef-only files just switch field types where free.
7. **XmFontList.c / XmRenderT.c internals**: `_XmGetFirstFont` and
   `_XmFontListSearch` keep public shapes; internals carry tokens.
8. **Gates**: extend `p1-draw-gate.sh` with the font pattern set →
   `tools/gate/p2-font-gate.sh`; zero violations required (FontS.c,
   XmIm.c, Screen.c-unit-font documented exemptions).

## Progress log

- 2026-09-06: survey complete (this file). Contract drafting next.
- 2026-09-06: contract landed — XmPlatTypes.h (kinds XFT/UTF8/32,
  XmPlatCharInfo, XmPlatColor), XmPlat.h (font metrics + string metrics +
  DrawStringColored + FontLoad/Unload + GC getters), XmPlatP.h (font-token
  constructors, backing/seam accessors), XmPlatDraw.c (metrics impls for
  FontStruct/FontSet/Xft, XftDraw cache moved from XmRenderT.c, UTF8/MB/WC/
  XFT draw paths, GC attribute getters).
- XmRenderT.c: _XmXftDrawCreate/Destroy/String2/String/SetClipRectangles/
  FontAverageWidth internals on the contract; XmRenderTableGetDefaultFontExtents
  via tokens; XftDraw cache deleted here (backend owns it).
- XmString.c: SubStringPosition/ComputeMetrics/_XmStringDraw text paths on
  the contract; Xutf8/Xmb/Xwc draw + XTextWidth/Extents + XGetGCValues gone.
  Line-metrics bug found by screenshot diff: XTextExtents' 2nd/3rd out params
  are FONT ascent/descent, not per-string ink — mapped via
  _XmPlatFontAscent/Descent.
- TextOut/TextF/DataF: FindWidth/FindHeight/PerCharExtents/FindPixelLength/
  init-metrics on prims; 46 per_char/min_bounds reads in TextOut gone.
- TextIn/Text/TabBox/List/I18List/ToggleB(G)/CascadeBG/Label(G)/Scale:
  metric reads on prims; Scale font load/unload via _XmPlatFontLoad/
  Unload; I18List falls back to XmRenderTableGetDefaultFontExtents in the
  non-XFT branch (old XmeRenderTableGetDefaultFont + field read there).
- XmFontList.c _XmGetFirstFont via _XmPlatFontSetFirstStruct seam.
- Bug found by periodic demo: _XmXftSetClipRectangles passed a NULL-GC ctx
  to _XmPlatSetClipRect; backend now guards NULL gc, wrapper uses the
  backend's XftDraw seam.
- Verification: c89 and c99 from clean, 0 warnings (p2-c89.log, p2-c99.log);
  both gates 0; screenshots match (hellomotif, periodic); UBSan runtime
  clean for both demos.
