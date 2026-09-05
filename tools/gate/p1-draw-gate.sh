#!/bin/sh
# Phase-1 contract gate (doc/plat-abstraction.md §3): fail if any direct
# core-X draw call appears in lib/Xm outside the allowed places.
# Allowed: lib/Xm/XmPlat/ (the backend itself), Xpm* + ImageCache.c
# (exempt until Phase 3).  Comment-only lines are ignored.
cd "$(dirname "$0")/../.." || exit 1
PAT='XDraw|XFill|XCopy|XPutImage|XCreateGC|XChangeGC|XSetClipMask|XClearArea'
tmp=$(mktemp /tmp/opencode/gate.XXXXXX)
rg -n "$PAT" lib/Xm -g '*.c' -g '!Xpm*' -g '!ImageCache.c' -g '!lib/Xm/XmPlat/*' > "$tmp" 2>/dev/null
# strip comment-only lines and lines inside block comments
python3 - "$tmp" <<'PYEOF'
import sys, re
hits=[]
for line in open(sys.argv[1]):
    line=line.rstrip('\n')
    parts=line.split(':',2)
    if len(parts)<3: continue
    code=parts[2]
    # remove block-comment text: naive — drop lines whose code part starts
    # (after optional whitespace) with '*' or '/*' or is inside comment
    stripped=code.strip()
    if stripped.startswith('*') or stripped.startswith('/*') or stripped.startswith('//'):
        continue
    if '/*' in code and '*/' in code and re.sub(r'/\*.*?\*/','',code).strip()=='':
        continue
    hits.append(line)
if hits:
    print("Phase-1 gate FAILED: direct X11 draw calls outside XmPlat/xpm/imagecache:")
    for h in hits: print(h)
    sys.exit(1)
print("Phase-1 gate OK: 0 violations")
PYEOF
rc=$?
rm -f "$tmp"
exit $rc
