# Phase 3 (Images) — Implementation Notes

Work log for Phase 3 of the platform abstraction (doc/plat-abstraction.md §3
Phase 3). Survey, design decisions, progress; final numbers go into the
plan's census record.

## Survey (2026-09-06, tree at commit ee16a8b)

### XImage users by subsystem

| Subsystem | Files | Role |
|---|---|---|
| Xpm library (embedded) | Xpmcreate, Xpmscan, XpmCr*, XpmWr*, XpmRd* (20 files) | XPM ↔ XImage/Pixmap converters + pixel put/get loops |
| ImageCache.c | 33 refs | `XmeGetImage`/`XmGetPixmap*` cache layer; `_XmPutScaledImage` (scaled blit, printing-aware) |
| TabBox.c | 38 refs | rotates tab pixmaps: `XGetImage` → `XiRotateImage` (XGetPixel/XPutPixel loops) → `XPutImage` → `XDestroyImage` |
| Readers | Png.c (2), Jpeg.c (1) | decode → `XCreateImage` (24-bit) |
| Producers of builtin bitmaps | DragIcon (4), MessageB (4), ReadImage.c (2), Obso2_0 (2) | `_XmCreateImage` macro (1-bit XYBitmap from bit data) |
| DataF.c | 3 | XGetImage of a stipple pixmap → XPutImage into 1-bit cursor |
| Region.c | 1 | `_XmRegionFromImage` (XGetPixel scan) |
| Backend | XmPlatDraw.c 1 (comment) | — |

### Raw call census (exit metric)

```
XCreateImage  7   (Xpmcreate 2, TabBox 2, Png 1, Jpeg 1, ImageCache 1)
XGetImage     8   (TabBox 4, DragIcon 2, DataF 1, ImageCache 1)
XPutImage     8   (TabBox 4, ImageCache 3, DataF 1)
XDestroyImage 39  (Xpm* 21, TabBox 9, ImageCache 4, DragIcon 3, DataF 1, ...)
XGetPixel     ~9  (TabBox 5, Xpmcreate 3, Region 1)
XPutPixel     ~11 (TabBox 5, Xpmcreate 6)
XSubImage     ~1  (Xpmcreate)
XAddPixel     0
XImageByteOrder/etc: XGetImage only reader
```

The Xpm pixel loops (PutImagePixels*, GetImagePixels*, PutPixel*, storePixel)
are the bulk of the `->data` direct accesses.

### What actually flows

1. **Load path**: file/data → XpmImage (Xpm* parse files) → XImage
   (Xpmcreate `CreateXImage` + PutImagePixels*) → Pixmap
   (`xpmCreatePixmapFromImage` = XCreatePixmap + XPutImage).  Png/Jpeg
   build 24-bit XImages directly.  Bitmaps build 1-bit XImages via
   `_XmCreateImage`.
2. **Cache path**: ImageCache caches XImage* (+mask, +pixels) per name;
   `XmGetPixmap`/`ByDepth` converts XImage→Pixmap (with depth fix-ups,
   `_XmPutScaledImage` for scaling/printing).
3. **Widget use**: mostly Pixmap (draw via Phase-1 `_XmPlatBlit`); the
   XImage level is touched only by TabBox (rotate), DataF (stipple copy),
   DragIcon (mask → region), Region (image → region), ImageCache (scaled
   put).

## Design decisions

### D1. XmPlatImage = the contract type; XImage is the X11 backend token

`XmPlatImage` (declared in Phase 1 as an opaque struct) becomes the
contract image type.  The X11 backend keeps the XImage as its backing
pointer — Phase 6 (cairo) swaps the backing for an image surface without
touching callers.

Seam (XmPlatP.h): `_XmPlatImageOf (XImage*)` already exists; add the
inverse `_XmPlatImageXImage (XmPlatImage)` for the transitional period,
plus `_XmPlatImageFree`.

### D2. Image prims in the contract

```c
/* Construction (pixel-buffer images, all depths) */
extern XmPlatImage _XmPlatImageCreate (XmPlatDrawCtx ctx, int depth,
                                       unsigned int width,
                                       unsigned int height);
/* ctx supplies display + visual + byte order; data is allocated by the
   backend and returned (caller fills via put-pixel prim or memcpy via
   the bytes accessor). */

extern void          _XmPlatImageFree (XmPlatImage image);

/* geometry + byte access */
extern unsigned int  _XmPlatImageWidth  (XmPlatImage image);
extern unsigned int  _XmPlatImageHeight (XmPlatImage image);
extern int           _XmPlatImageDepth  (XmPlatImage image);
extern char *        _XmPlatImageData   (XmPlatImage image);      /* raw bytes */
extern int           _XmPlatImageBytesPerLine (XmPlatImage image);

/* pixel access (the XGetPixel/XPutPixel replacements) */
extern unsigned long _XmPlatImageGetPixel (XmPlatImage image, int x, int y);
extern void          _XmPlatImagePutPixel (XmPlatImage image, int x, int y,
					   unsigned long pixel);

/* sub-image copy */
extern XmPlatImage   _XmPlatImageSub (XmPlatImage image, int x, int y,
				      unsigned int w, unsigned int h);

/* screen I/O */
extern XmPlatImage   _XmPlatImageFromSurface (XmPlatDrawCtx ctx,
					      XmPlatSurface src,
					      int src_x, int src_y,
					      unsigned int w,
					      unsigned int h);
/* replaces XGetImage; ctx carries the GC (plane mask) */
```

`_XmPlatPutImage` (Phase 1) already covers XPutImage.

### D3. Who moves, who stays

- **TabBox.c**: `XiRotateImage` + the three XGetImage/XPutImage/
  XDestroyImage sites → prims.  Its pixel loops become
  GetPixel/PutPixel prims (backend keeps XImage's fast native access).
- **DataF.c**: XGetImage/XDestroyImage → `_XmPlatImageFromSurface` +
  `_XmPlatImageFree`; image->width/height via accessors; `image->data`
  not used here.
- **DragIcon.c**: `_XmCreateImage` (macro) → new seam function
  `_XmPlatImageCreateBitmap (dpy, data, w, h)` (1-bit); XGetImage →
  `_XmPlatImageFromSurface`.
- **MessageB.c**: `_XmCreateImage` → same bitmap seam.
- **ReadImage.c / Obso2_0.c**: `_XmCreateImage` → bitmap seam
  (public `_XmReadImageAndHotSpotFromFile` keeps returning XImage*
  because it is frozen API? — NO: it is internal (`_Xm` prefix, declared
  in ReadImageI.h, used only by ImageCache + Obso2_0).  Return
  XmPlatImage internally; Obso2_0's wrappers cast at the boundary).
- **Region.c**: `_XmRegionFromImage(XImage*)` → internal, declared in
  RegionI.h; switches to XmPlatImage + GetPixel prim.
- **ImageCache.c**: XCreateImage → prims; XPutImage → `_XmPlatPutImage`;
  XGetImage → FromSurface; XDestroyImage → Free.  The scaled-blit
  `_XmPutScaledImage` keeps its strip logic, calling prims.
- **Png.c / Jpeg.c**: XCreateImage → `_XmPlatImageCreate` + fill via
  `->data` bytes (kept: the decoders write packed rows; keep
  `_XmPlatImageData`/BytesPerLine accessors for them).
- **Xpm subsystem (20 files)**: the plan says these collapse into
  `XmImage + backend blit`.  Realistic Phase-3 scope: keep the Xpm parse
  layer (file format code, pure CPU) and convert its XImage *touch
  points* to prims: `CreateXImage`, `PutImagePixels*`/`GetImagePixels*`
  (they walk pixels — use PutPixel/GetPixel prims on the token),
  `xpmCreatePixmapFromImage` (XCreatePixmap + PutImage → surface +
  `_XmPlatPutImage`), XDestroyImage → `_XmPlatImageFree`.  The pixel
  loops in Xpmcreate.c/Xpmscan.c are performance-sensitive; the backend
  GetPixel/PutPixel prims preserve the XImage fast paths (32/16/8/1-bit
  loops stay in the backend where they can use ->data directly).
  Full data-layout rework of Xpmparse → deferred to Phase 6 where cairo
  image surfaces replace the whole conversion layer.

### D4. Public API surface touched

- `XmeGetImage` / `XmGetPixmap(ByDepth)` signatures keep XImage*/Pixmap —
  frozen.  Internally the cache stores XmPlatImage tokens; the XImage
  exits/enters only at the API boundary via `_XmPlatImageOf`.
- `_XmCreateImage` macro (XmI.h) becomes a wrapper over the bitmap seam
  (macro body changes; macro name stays for in-tree compat).
- `XmeXpm*` (XpmP.h) re-exports of the embedded xpm lib: signatures
  keep XImage* — they are "public" through XmeXpm aliases; conversion
  at the boundary.

### D5. Gate

`tools/gate/p3-image-gate.sh`: pattern
`XCreateImage|XDestroyImage|XGetImage|XPutImage|XGetSubImage|XGetPixel|XPutPixel|XSubImage|XAddPixel|XImageByteOrder`
outside XmPlat/; exemptions per above.  Zero violations required.

## Progress log

- 2026-09-06: survey complete (this file).
- 2026-09-06: contract landed — XmPlat.h image section (Create,
  CreateOnVisual, CreateBitmap, Free, Width/Height/Depth/Data/
  BytesPerLine, GetPixel/PutPixel, Sub, FromSurface); XmPlatDraw.c
  backend (XImage-backed tokens; _XmPlatImageRec struct moved to the
  top of the file).  Seam: _XmPlatImageTokenOf / _XmPlatImageXImage /
  _XmPlatImageTokenFree (disown) / _XmPlatImageBitmapOf /
  _XmPlatImageFromSurface2 / _XmPlatImageRawCreate.
- XmPlatImage typedef moved to XmPlatTypes.h (the old Phase-1 static
  cast seam is gone - tokens are structs now).
- TabBox: XiRotateImage on prims (OnVisual variant keeps the shell
  visual); 4 XGetImage, 9 XDestroyImage, 8 PutImage call sites on
  tokens; pixel loops on Get/PutPixel prims.
- DataF: stipple-copy XGetImage/PutImage/DestroyImage on tokens.
- DragIcon: bitmap seam + FromSurface2 for mask->region; bug caught by
  the harness: _XmInstallImage KEEPS the XImage, so the token must be
  disowned (TokenFree), not freed (ImageFree) - the first version
  XDestroyImage'd installed builtin icons, which surfaced as BadMatch
  on a later XPutImage and a black periodic window.
- MessageB: CreateDefaultImage via bitmap seam, XImage out at the
  public XmInstallImage boundary.
- ReadImage/Obso2_0: bitmap seam; frozen XImage*-returning internals
  unwrap the token.
- Region: _XmRegionFromImage takes XmPlatImage (RegionI.h includes
  XmPlatTypes.h).
- ImageCache: PutImage sites + strip-scaler pixels on the contract;
  XCreateImage of the strip dest via _XmPlatImageRawCreate seam;
  XDestroyImage wrapped.  The cache's XImage field bookkeeping
  (->data/->format swaps) stays - it is cache-internal depth fix-up.
- Png/Jpeg: XCreateImage via raw-create seam.
- Xpm: X11 bridge points converted (XpmCrIFrP XGetImage, XpmCrPFrI
  XCreateGC/XPutImage via ctx-on-surface); the parse/pixel engine
  (Xpmcreate PutPixel*, Xpmscan) stays XImage-internal per plan
  (Phase-6 collapse).
- Verification: c89 and c99 from clean, 0 warnings; p3 gate 0 (p1/p2
  gates still 0); screenshots match (hellomotif, periodic); UBSan
  runtime clean for both demos.
