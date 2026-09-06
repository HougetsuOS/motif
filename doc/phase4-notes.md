# Phase 4 notes — events (XmEvent contract)

Status: in progress.  Plan: doc/plat-abstraction.md §Phase 4.
Goal: widgets stop reading XEvent fields; Xt stays the event source; the
X11/Xt backend performs the one translation at dispatch boundary time.

## 1. Survey (census at 2.3.8 + this tree)

- 1,809 raw `XEvent` line-matches in 87 files (plan baseline 1,811/87).
- Top: TextIn 258, DataF 233, TextF 159, List 116, Container 83, TabBox 49,
  RCMenu 46, SpinB 38, ScrollBar 36 ... long tail of 49 files ≤10 lines.
- Xt event handler registrations (`XtAddEventHandler`): 68 sites.
- Direct event-loop bypasses (XNextEvent family): 14 files
  (DragC, DragICC, TrackLoc, TearOff, Tree, Outline, Display, MenuShell,
  CutPaste, ValTime, DataF, PrintS, TextIn comment, DropTrans via ICC).
  These stay: the plan keeps Xt as event source and these are
  selection/DnD/protocol plumbing; they consume XEvent *inside* the
  plumbing layer, not in widget field-reading code.

### Field census (what widgets actually read)

  xbutton: time 105, x 90, y 73, y_root 26, x_root 26, button 16,
           window 8, state 8
  xkey:    time 102, x 5, y 5, state 4, keycode 3
  xmotion: x 35, y 28, x_root 7, y_root 7, subwindow 7, window 6,
           time 6, state 3, is_hint 2, root 2
  xcrossing: focus 16, detail 9, mode 7, x 6, y 4, subwindow 4, time 5
  xexpose: x 12, y 12, width 7, height 7
  xfocus:  send_event 19 (!), detail 2
  xconfigure: x 2, y 1, width 1, height 1, border_width 1
  xselection: time 3, requestor 1, property 1  (Phase-5 selection plumbing)
  xany:    type 43, window 14, serial 5, display 2, send_event 1
  type via `event->type`: 109 sites
  VirtKeys.c: XKeycodeToKeysym for keycode→keysym (keyboard mapping).

### Fabrication sites (synthetic events, the hard part)

- GadgetUtil.c `_XmDispatchGadgetInput`: copies a union member of the
  incoming event into a stack XEvent, optionally rewrites `.type`, then
  hands it to the gadget's input_dispatch.  The *point* of the rewrite is
  the type + the union member copy; gadgets read the same fields after.
- MenuUtil.c: fabricates FocusOut (xfocus.send_event=True) to force
  manager focus-out; RCMenu.c:3528 same pattern with an xcrossing.
- Protocols.c:312: bzero'd XEvent + XtDispatchEvent for WM_PROTOCOLS.
- PrintS.c:1104: fabricates Expose for print-shell repaint.
- Transfer/CutPaste/DataFSel/TravAct/DragC/TearOff/TrackLoc: event-loop
  helpers that XCheck*/XtDispatchEvent real events (consumption + redispatch).
- DataF.c:7596, TextIn.c:4010, TextF.c:5891: heap-copy the event into the
  transfer-action record for later replay.
- Container.c:4103: XtCalloc'd XEvent stored in a DnD payload.

### Public-header constraints (frozen API)

- `XmP.h`: `typedef void (*XmWidgetDispatchProc)(Widget, XEvent *, Mask)`
  — gadget `input_dispatch` and primitive/manager action procs are typed
  on XEvent* in the *frozen* public headers.  GadgetP.h embeds it.
- Xt action procs and event handlers keep XEvent* (Xt's own signatures).
- `XmImMbLookupString(Widget, XKeyEvent*, ...)` — IM API frozen; TextF
  casts.  XmIm.c itself takes XEvent* only in one internal callback.

So "zero XEvent outside XmPlat" must mean **zero XEvent *field access***
in widget code; XEvent* may remain as an opaque pointer passed through
frozen signatures into the seam, per the plan's exit criterion wording.

## 2. Design

### D1 — the token: `XmPlatEvent` (same pattern as Phase 2/3)

Opaque-ish handle in XmPlatTypes.h, X11 definition in the backend:

```c
typedef struct _XmPlatEventRec *XmPlatEvent ;
```

The X11 backend token simply holds `const XEvent *ev` (not a copy) —
translation is read-on-demand via prims, so there is exactly one
conversion point per *field family*, at the seam, and no widget ever
sees a field.  (An eager "translate at dispatch time" record was
rejected: it would force the backend to enumerate every field family
per event type up-front — more code, and Phase-6/7 backends differ in
what they can produce.)

### D2 — prims (XmPlat.h "Phase 4" section)

Kinds mirror the plan's union tags, spelled XmPlatEvent*:

  _XmPlatEventKind _XmPlatEventKind(ev)   -- None/Pointer/Key/Crossing/
                                             Focus/Expose/Configure/
                                             Map/Unmap/Property/
                                             ClientMessage/Selection/
                                             Visibility/Reparent/Other
  Boolean _XmPlatEventIsType(ev, kind)    -- replaces `event->type ==`
  int      _XmPlatEventSerial(ev)
  Boolean  _XmPlatEventSendEvent(ev)      -- xany.send_event + xfocus's
                                             send_event reads
  void*    _XmPlatEventWindow(ev)         -- opaque window token (X11:
                                             Window).  Read sites are
                                             DnD/selection plumbing and
                                             stay XEvent-internal per D4.
  XmPlatTime _XmPlatEventTime(ev)         -- xbutton/xkey/xmotion/
                                             xcrossing/xproperty time;
                                             0 (CurrentTime) elsewhere

  Pointer family (button/motion):
    x, y, root_x, root_y, button (0 if none), state, is_hint
  Key family:
    keycode, state, keysym (backend maps keycode via XLookupKeysym)
  Crossing family:
    focus (Boolean), mode, detail, x, y, subwindow-opaque
  Expose family: x, y, width, height, count
  Configure family: x, y, width, height
  Focus family: detail, mode
  Property family: atom-opaque (string name via XmAtom in Phase 5)
  Visibility: fully-obscured Boolean
  Map/Unmap: no extra fields used

Window/atom/subwindow reads are opaque *tokens* (XmPlatWindow), not
`Window`; widgets that only test "is it my window" compare
_XmPlatEventWindow(ev) against _XmPlatWindowOf(w).  Phase 5 will unify
window tokens; for Phase 4 the token is the X11 Window cast to a
pointer-sized integer (XmPlatWindow typedef, backend-defined).

### D3 — where the seam lives

- Xt action procs / event handlers / input_dispatch keep receiving
  XEvent* (frozen signatures).  The FIRST statement of each handler
  wraps: `XmPlatEvent pev = _XmPlatEventOf(event) ;` and the body reads
  prims.  `_XmPlatEventOf` is a seam (X11: identity on the pointer).
- Gadget `_XmDispatchGadgetInput` fabricates tokens the same way it
  fabricates XEvents today: build an XEvent copy internally (backend
  helper `_XmPlatEventSynth(ev, kind, new_kind)`) and hand both the raw
  pointer (for the frozen signature) and its token to the gadget.
  Gadget input_dispatch implementations read prims.
- Synthetic FocusOut/Expose/WM_PROTOCOLS fabrication sites (MenuUtil,
  Protocols, PrintS, RCMenu) use the same synth helper.
- Event replay copies (DataF/TextIn/TextF transfer records) store the
  token + the backend keeps the raw XEvent alive for the record's
  lifetime (the record already owns a heap copy today — the copy moves
  into the backend: `_XmPlatEventCopy(ev)` returns an owning token,
  `_XmPlatEventFreeCopy` releases).

### D4 — scope cut (what stays XEvent-internal)

Same rule as Phase 3's Xpm exemption: the event *plumbing* that exists
to move events around (event-loop helpers, selection/DnD dispatchers,
IM filter, Xt itself) keeps raw XEvent; widget *field reads* move onto
the contract.  Specifically exempt (documented):
  - CutPaste, Transfer, DataFSel, TextFSel, DragC/DragICC/DropSMgr/
    DropTrans (DnD + selection transport; Phase-5 atoms land here next)
  - TrackLoc, TearOff, Display.c, ValTime, Protocols, PrintS, MenuShell
    peek loops (event-loop helpers; MenuShell's XPeekEvent stays)
  - XmIm.c (IM filter; XKeyEvent* is the frozen XmIm API)
  - VirtKeys.c (keycode→keysym is the backend's job; its XKeycodeToKeysym
    moves behind a backend helper or is absorbed into the Key prim)
  - EditresCom.c (Xt protocol, not widget logic)
  - ShellE.c/MenuShell.c/PrintS.c shell internals (Xt shell machinery)
- The gate (§5) therefore scans for XEvent FIELD reads and the
  fabrication pattern, not the bare `XEvent` token.

### D5 — gate

tools/gate/p4-event-gate.sh, same block-comment-aware stripper:
  patterns: `->type` on XEvent vars is inexpressible in grep; so the
  gate flags:
    `->xany\.`, `->xkey\.`, `->xbutton\.`, `->xmotion\.`,
    `->xcrossing\.`, `->xfocus\.`, `->xexpose\.`, `->xconfigure\.`,
    `->xvisibility\.`, `->xmap\.`, `->xunmap\.`, `->xproperty\.`,
    `->xclient\.`, `->xselection\.`, `->xreparent\.`, `->xcolormap\.`,
    and `XEvent [A-Za-z_]` declarations in widget code, plus
    `XSendEvent|XPutBackEvent` outside the exempt plumbing list.
  Exemptions: XmPlat/*, the D4 plumbing list, and XEvent* appearing in
  frozen signatures (declarations `XEvent *` as parameters are fine —
  only field reads and local declarations are violations).

## 3. Migration order (census, long tail batched)

Batch 1 (simple, ≤15 lines, field reads only): Label, LabelG, PushB,
PushBG, ToggleB, ToggleBG, ArrowB, ArrowBG, DrawnB, IconButton, Sash,
TearOffB, Primitive, Manager, CascadeB, CascadeBG, RowColumn, RCPopup,
MenuUtil, TravAct, GadgetUtil, ComboBox, DropDown, SpinB, ScrollBar,
List, I18List, TabStack, TabBox, Container, ScrolledW, GrabShell,
DataFSel (handler reads only), TextOut, UniqueEvnt, DragC (handler
reads), RCMenu, MenuShell, MenuUtil, XmStringFunc, Screen, Display
(handler reads), Outline, Tree (handler reads), DrawingA, TrackLoc
(handler reads), CutPaste (handler reads), Transfer (handler reads).

Batch 2 (the minefield, after batch 1 is green): TextIn, DataF, TextF.

XmIm, VirtKeys, Protocols, EditresCom, TearOff event loops, ValTime,
PrintS internals: documented D4 exemptions.

## 4. Verification

- p4 gate at 0 (with documented exemptions)
- p1/p2/p3 gates still 0
- c89/c99 from clean, 0 warnings
- screenshots match (hellomotif + periodic)
- UBSan runtime clean both demos
- keyboard-interaction smoke: periodic keyboard navigation still works
  (screenshots cover it; manual check if anything looks off)

- 2026-09-06: contract landed — XmPlatEvent token (XmPlatTypes.h), kinds
  (Pointer/Key/Crossing/Focus/Expose/.../Other), field prims (time, x/y,
  root x/y, button/state/is-hint, keycode/keysym, crossing focus/mode/
  detail/subwindow, expose width/height/count, configure, visibility,
  property-name/requestor) + direction helpers (IsEnter/IsLeave/
  IsFocusIn/IsFocusOut/IsKeyPress/IsKeyRelease/PointerKind) + writable
  SetX/SetY (DoGrabFocus clamping) + Copy/Synth/FreeCopy/Raw seam.
  XmPlatWindow token (X11 Window as unsigned long) for "is this event
  on my window?" tests; XmPlatTime for timestamps.
- Gadget dispatch (_XmDispatchGadgetInput) now builds synth tokens per
  mask kind; input_dispatch implementations receive the rewritten raw
  record through the frozen signature exactly as before, so gadgets
  read prims against a copy identical in semantics to the old stack
  CopyEvent + type rewrite.
- Migration: all non-Text widget field readers + the Text trio
  (TextIn 258, DataF 233, TextF 159 raw counts) moved onto prims.
  The three Text action-switch sites (ProcessBSelect) keep a single
  XmPlatEventPointer case with is-press/is-release/is-motion chains
  (Kind collapses press/release/motion by design).
- Bugs caught during migration: WritePattern regex over-reach on
  event-coord WRITE sites (TextIn DoGrabFocus, DataF transfer records,
  TextF ABS_DELTA press fields) — fixed with SetX/SetY prims and
  token-wrapping of the stored transfer-action events; duplicate enum
  values between kind/sub-kind enums fixed by basing pointer sub-kinds
  at 100; XSelectionClearEvent field is .selection not .atom.
- Gate: tools/gate/p4-event-gate.sh — scans union-member access,
  direct .type compares on event vars, with the D4 exemption list.
- Verification: c89/c99 from clean 0 warnings; p1-p4 gates 0;
  screenshots match; UBSan runtime clean (hellomotif, periodic).
