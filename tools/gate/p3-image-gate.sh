#!/bin/sh
# Phase-3 contract gate (doc/plat-abstraction.md §3 Phase 3): fail if any
# direct core-X image call appears in lib/Xm outside the allowed places.
#
# Allowed:
#   - lib/Xm/XmPlat/                      (the backend itself)
#   - Xpm* (all): the embedded Xpm subsystem is the self-contained
#     pixel engine the plan schedules for the Phase-6 collapse (cairo
#     image surfaces replace the conversion layer wholesale).  Its X11
#     *bridge* points (XGetImage/XPutImage/XCreateGC) are already on
#     the contract.
#   - ImageCache.c XImage field bookkeeping (->data/format swaps are the
#     cache's depth-fixup logic); its X calls go through the contract.
# Comment-only lines are ignored.
cd "$(dirname "$0")/../.." || exit 1
PAT='XCreateImage|XDestroyImage|XGetImage|XPutImage|XGetSubImage|XGetPixel|XPutPixel|XSubImage|XAddPixel|XImageByteOrder'
tmp=$(mktemp /tmp/opencode/gate3.XXXXXX)
rg -n "$PAT" lib/Xm -g '*.c' -g '!lib/Xm/XmPlat/*' -g '!Xpm*' > "$tmp" 2>/dev/null
python3 - "$tmp" <<'PYEOF'
import sys, re

files={}
for line in open(sys.argv[1]):
    line=line.rstrip('\n')
    parts=line.split(':',2)
    if len(parts)<3: continue
    files.setdefault(parts[0],[]).append((int(parts[1]),parts[2]))

# comment map per file
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
    print("Phase-3 gate FAILED: direct X11 image calls outside XmPlat/Xpm-engine:")
    for h in hits: print(h)
    sys.exit(1)
print("Phase-3 gate OK: 0 violations")
PYEOF
rc=$?
rm -f "$tmp"
exit $rc