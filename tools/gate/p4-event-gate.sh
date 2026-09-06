#!/bin/sh
# Phase-4 contract gate (doc/plat-abstraction.md §3 Phase 4): fail if any
# XEvent FIELD access or event fabrication appears in widget code outside
# the allowed places.
#
# Allowed (documented in doc/phase4-notes.md §D4):
#   - lib/Xm/XmPlat/                    the backend itself
#   - DragC, DragICC, DropSMgr, DropTrans, CutPaste, Transfer
#                                       DnD + selection transport (Phase-5
#                                       atoms land here next)
#   - TrackLoc, TearOff, Display, ValTime, Protocols, PrintS, MenuShell
#                                       event-loop helpers / peek loops
#   - XmIm                              IM filter (frozen XmKeyEvent* API)
#   - VirtKeys                          keycode->keysym (backend's job)
#   - EditresCom                        Xt protocol, not widget logic
#   - ShellE, MenuShell (synth paths)   Xt shell machinery
#   - RowColumn fast-expose XEvent fabrication (feeds frozen
#     XmeRedisplayGadgets signature); RCPopup xany.window munge around
#     XtDispatchEvent (replay plumbing)
# Comment-only lines are ignored.
cd "$(dirname "$0")/../.." || exit 1
PAT='->xany\.|->xkey\.|->xbutton\.|->xmotion\.|->xcrossing\.|->xfocus\.|->xexpose\.|->xconfigure\.|->xvisibility\.|->xmap\.|->xunmap\.|->xproperty\.|->xclient\.|->xselection\.|->xreparent\.|->xcolormap\.|->xgraphicsexpose\.|->xcreatewindow\.|->xresizerequest\.|->xcirculate\.|->xnoexpose\.|event->type|event -> type|ev->type|pev->type|e -> type|e->type|->event->type'
EXEMPT='DragC|DragICC|DropSMgr|DropTrans|CutPaste|Transfer|TrackLoc|TearOff|Display|ValTime|Protocols|PrintS|MenuShell|XmIm|VirtKeys|EditresCom|ShellE'
tmp=$(mktemp /tmp/opencode/gate4.XXXXXX)
rg -n "$PAT" lib/Xm -g '*.c' -g '!lib/Xm/XmPlat/*' > "$tmp" 2>/dev/null
python3 - "$tmp" "$EXEMPT" <<'PYEOF'
import sys, re

exempt = sys.argv[2].split('|')
files={}
for line in open(sys.argv[1]):
    line=line.rstrip('\n')
    parts=line.split(':',2)
    if len(parts)<3: continue
    base=parts[0].rsplit('/',1)[-1].replace('.c','')
    if base in exempt or 'XmIm' in parts[0]:
        continue
    files.setdefault(parts[0],[]).append((int(parts[1]),parts[2]))

in_comment={}
for f,entries in files.items():
    try: src=open(f).read().split('\n')
    except OSError: src=[]
    inc=False; m={}
    for i,text in enumerate(src,1):
        if inc:
            m[i]=True
            if '*/' in text: inc=False
        else:
            j=text.find('/*')
            if j>=0:
                m[i]=True
                if '*/' not in text[j:]: inc=True
    in_comment[f]=m

hits=[]
for f,entries in files.items():
    for num,code in entries:
        if in_comment.get(f,{}).get(num,False): continue
        stripped=code.strip()
        if stripped.startswith('*') or stripped.startswith('//'):
            continue
        hits.append(f"{f}:{num}:{code}")

if hits:
    print("Phase-4 gate FAILED: XEvent field access outside XmPlat/plumbing:")
    for h in hits: print(h)
    sys.exit(1)
print("Phase-4 gate OK: 0 violations")
PYEOF
rc=$?
rm -f "$tmp"
exit $rc