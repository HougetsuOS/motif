# Phase 5 notes — atoms / properties (XmAtom contract)

Status: in progress.  Plan: doc/plat-abstraction.md §Phase 5.
Goal: atom interning, property read/write, and WM-protocol event sending
go through the contract; widget code stops calling X* atom/property APIs.

## 1. Survey (census at this tree, §5 regex + widened scan)

- Raw atom/property API use (XInternAtom/XmInternAtom/XChangeProperty/
  XGetWindowProperty/XDeleteProperty/XSendEvent/XGetAtomName): 445
  line-matches in 57 files (plan baseline 193/38 with narrower regex).
- Interning: XmInternAtom (AtomMgr.c) is already a 2-line facade over
  XInternAtom; Screen.c/Display.c use XInternAtoms bulk; most of the 445
  are XmInternAtom calls in selection/DnD/text plumbing.
- Property I/O clusters: CutPaste.c 13, Text.c 8, DragICC.c 8, DragBS.c 8,
  VirtKeys.c 3, Display.c 3, DataF.c 3, Transfer.c 2, TearOff.c 2,
  Protocols.c 2, VendorSE/VendorS 1 each.
- XSendEvent sites: CutPaste.c (multi-chunk transfer), DragICC.c (DnD
  protocol messages), Protocols.c (WM_PROTOCOLS client messages),
  PrintS.c (protocol), MenuUtil/UniqueEvnt/Transfer (synthetic dispatch).

### Public-header constraints (frozen API)

- `Atom` appears in frozen public structs (XmConvertCallbackStruct.target,
  XmSelectionCallbackStruct, XmDragTransfer...), resources (XmNselection*),
  and the XmInternAtom/XmGetAtomName signatures.  The public type `Atom`
  stays; only *where the integer is produced and consumed* moves.

## 2. Design

### D1 — `XmPlatAtom` token (XmPlatTypes.h)

```c
typedef struct _XmPlatAtomRec *XmPlatAtom ;
```

X11 backend: the token holds the interned `Atom` plus a cached name
pointer (interning is per-display in X11; the token keeps both so
name<->id translation is free after first use).  None token = NULL.

Prims (XmPlat.h "Phase 5" section):
  XmPlatAtom _XmPlatAtomIntern (Display *dpy, const char *name,
                                Boolean only_if_exists)
  const char *_XmPlatAtomName (XmPlatAtom a)      /* malloc'd */
  Atom        _XmPlatAtomRaw (XmPlatAtom a)       /* seam, frozen APIs */
  XmPlatAtom  _XmPlatAtomOfRaw (Display *dpy, Atom raw)  /* seam inverse */
  void        _XmPlatAtomFree (XmPlatAtom a)      /* frees name cache */

  Display *dpy leaks into these seams exactly like _XmPlatFontLoad —
  the plan already tolerates Display* in lifecycle seams (§contract
  header note); the cairo/test backends intern names locally.

### D2 — property prims

  _XmPlatChangeProperty (Display*, Window, XmPlatAtom prop,
        XmPlatAtom type, int format, int mode, data, nelements)
  _XmPlatGetWindowProperty (Display*, Window, XmPlatAtom prop,
        long offset, long length, int delete,
        /* out */ XmPlatAtom *ret_type, int *ret_format,
        unsigned char **data, unsigned long *nitems,
        unsigned long *bytes_after)
  _XmPlatDeleteProperty (Display*, Window, XmPlatAtom prop)
  _XmPlatSendClientMessage (Display*, Window, Boolean propagate,
        long event_mask, XEvent *msg)   /* raw msg; fabrication seam */

Window here is the raw X11 Window for now (plumbing layer); a full
XmPlatWindow unification is Phase-5-end / Phase-6 cleanup — the DnD and
selection transport files are D4-style plumbing anyway.

### D3 — migration scope (same shape as Phase 4)

Migrate onto the contract:
  - the atom/property call sites in the selection + DnD transport files
    (CutPaste, Transfer, DragBS, DragICC, DragC, TextSel, TextFSel,
    DataFSel, DataF, TextF, TextIn, TextOut, Text, Container, TabStack,
    TabBox, Container, Label/Scale drag icons, ColorObj, TxtPropCv,
    IsMwmRun, Xm.c registry atoms, Screen/Display bulk interning,
    ResEncod, ResConvert, XmString, TearOff, VendorS/E, Protocols,
    SelectioB, FileSB, I18List, List, ValTime, PrintS, EditresCom)
  - XmInternAtom/XmGetAtomName become wrappers over the contract prims
    (source-compat; all internal callers switch to _XmPlatAtom*).

Exemptions (documented, same reasoning as Phase 4 D4):
  - XmPlat/* (the backend)
  - Xm.c font-registry charset atoms? no — convert via contract too.
  - The synthetic ClientMessage fabrication (DragICC, Protocols,
    CutPaste multi-chunk, PrintS) constructs raw XEvent structs to fill
    the .type + payload; they use _XmPlatSendClientMessage with the
    fabricated message struct (the struct itself stays XEvent — it is
    the wire format, like Xpm's pixel engine).

### D4 — gate

tools/gate/p5-atom-gate.sh: pattern XInternAtom|XInternAtoms|XmInternAtom
(exempt callers inside AtomMgr wrapper)|XChangeProperty|XGetWindowProperty|
XDeleteProperty|XSendEvent|XGetAtomName|XmGetAtomName; exemptions XmPlat/*
+ the fabrication/transport set if any remain (goal: none).

## 3. Migration order

Batch 1 (atom interning only — mechanical): everything calling
XmInternAtom/XInternAtom to build Atom variables.  These keep the
integer in widget records (public structs) but obtain it via the
contract.  NOTE: this alone does not remove XInternAtom from the tree —
the *backend* does the interning.

Batch 2 (property I/O): CutPaste (13), Text (8), DragICC (8), DragBS (8),
VirtKeys (3), Display (3), DataF (3), Transfer, TearOff, Protocols,
VendorS/SE, Screen, ValTime.

Batch 3 (XSendEvent): Protocols, DragICC, CutPaste, PrintS, MenuUtil.

## 4. Verification

- p5 gate at 0; p1-p4 gates still 0
- c89/c99 from clean, 0 warnings
- screenshots match; UBSan runtime clean
- selection smoke: clipboard copy/paste inside hellomotif text field
  (manual; screenshots cannot cover selection), periodic OK

- 2026-09-06: contract landed — XmPlatAtom token (Atom + cached name +
  owning Display) with Intern/Name/Raw/OfRaw/Free, and the property
  family: ChangeProperty, GetWindowProperty (Xlib arg order, raw atom
  ids in/out), DeleteProperty, SendClientMessage (raw msg record — the
  XEvent wire format is fabrication territory like Xpm's pixel engine),
  RotateBuffers (cut-buffer kill ring), Sync.
- Migration glue used at call sites: _XmPlatInternAtomRaw /
  _XmPlatInternAtomsRaw / _XmPlatAtomNameRaw (raw Atom in/out so the
  frozen `Atom`-typed widget fields and public structs are untouched;
  128 single-intern sites + 56 bulk sites + 8 name sites rewritten).
- The public wrappers XmInternAtom/XmGetAtomName now route through the
  backend; their signatures are frozen.
- Property prims deliberately take RAW atom ids + raw window handles:
  CutPaste/Transfer/DragBS/DragICC are plumbing that holds Atoms in
  widget records; token-wrapping every field was rejected (Phase-6
  cleanup owns the window unification).
- Gate: tools/gate/p5-atom-gate.sh at 0 (AtomMgr wrapper bodies
  inspected; Transfer.c's "XGetAtomName" string constant excluded).
- Verification: c89/c99 from clean 0 warnings; p1-p5 gates 0;
  screenshots match; UBSan runtime clean (hellomotif, periodic).
  Selection smoke (copy/paste in hellomotif) to be checked interactively
  by the user; the clipboard property paths exercised by the demos'
  build/destroy are clean under UBSan.
