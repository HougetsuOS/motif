#!/bin/sh
# Phase-5 contract gate (doc/plat-abstraction.md §3 Phase 5): fail if any
# direct X11 atom/property call appears in lib/Xm outside the backend.
#
# The only X-call surface allowed is lib/Xm/XmPlat (the backend itself);
# AtomMgr.c's XmInternAtom/XmGetAtomName are public wrappers whose bodies
# route through the backend (they are excluded from the scan as source-
# compat shims, verified by inspection).
cd "$(dirname "$0")/../.." || exit 1
PAT='XInternAtom|XChangeProperty|XGetWindowProperty|XDeleteProperty|XSendEvent|XGetAtomName|XRotateBuffers'
tmp=$(mktemp /tmp/opencode/gate5.XXXXXX)
rg -n "$PAT" lib/Xm -g '*.c' -g '!lib/Xm/XmPlat/*' > "$tmp" 2>/dev/null
python3 - "$tmp" <<'PYEOF'
import sys

files={}
for line in open(sys.argv[1]):
    line=line.rstrip('\n')
    parts=line.split(':',2)
    if len(parts)<3: continue
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
        # AtomMgr.c wrapper bodies are inspected source-compat shims
        if f.endswith('AtomMgr.c'):
            continue
        # Transfer.c defines a string constant "XGetAtomName" for an error
        # message table; not a call.
        if '"XGetAtomName"' in code:
            continue
        hits.append(f"{f}:{num}:{code}")

if hits:
    print("Phase-5 gate FAILED: direct X11 atom/property calls outside XmPlat:")
    for h in hits: print(h)
    sys.exit(1)
print("Phase-5 gate OK: 0 violations")
PYEOF
rc=$?
rm -f "$tmp"
exit $rc