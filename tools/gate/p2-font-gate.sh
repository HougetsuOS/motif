#!/bin/sh
# Phase-2 contract gate (doc/plat-abstraction.md §3 Phase 2): fail if any
# direct core-X font/text call or metric access appears in lib/Xm outside
# the allowed places.
#
# Allowed:
#   - lib/Xm/XmPlat/           (the backend itself)
#   - Xpm*, ImageCache.c       (exempt until Phase 3; they don't draw text)
#   - Documented exemptions (see doc/phase2-notes.md §D6):
#       FontS.c                (font-selector UI: its job IS enumerating
#                               core fonts; migrates when the default-font
#                               story settles)
#       XmIm.c                 (XIM input context: XFontSet is part of the
#                               X11 XIM API; revisited in Phase 5)
#       Screen.c               (font-unit computation; 1 site, documented)
#   - XmbTextListToTextProperty family: selection/COMPOUND_TEXT machinery
#     = Phase 5, not gated here.
# Comment-only lines are ignored.
cd "$(dirname "$0")/../.." || exit 1
# XGetFontProperty: kept as the quad-width fallback (TextF/TextOut/DataF,
#   documented in doc/phase2-notes.md §D6) - excluded from PAT.
# XftFontMatch/OpenPattern/Close: rendition font load/unload lifecycle in
#   XmRenderT (the converter layer; stays until Phase 3's XmImage/Phase-6
#   font work) - excluded from PAT.
# _XmXft* legacy wrappers in XmRenderT (lines 2873-3095): source-compat
#   shims whose bodies are already on the contract - excluded via sed.
PAT='XmbDrawString|XmbDrawImageString|XwcDrawString|XwcDrawImageString|Xutf8DrawString|Xutf8DrawImageString|XDrawString|XDrawImageString|XTextWidth|XTextExtents|XmbTextEscapement|XmbTextExtents|XwcTextEscapement|XwcTextExtents|Xutf8TextEscapement|Xutf8TextExtents|XFontsOfFontSet|XLoadQueryFont|XFreeFont|XftDraw|XftTextExtents|XGetGCValues'
tmp=$(mktemp /tmp/opencode/gate2.XXXXXX)
rg -n "$PAT" lib/Xm -g '*.c' \
   -g '!Xpm*' -g '!ImageCache.c' \
   -g '!lib/Xm/XmPlat/*' \
   -g '!lib/Xm/FontS.c' -g '!lib/Xm/XmIm.c' -g '!lib/Xm/Screen.c' \
   > "$tmp" 2>/dev/null
# drop the legacy-wrapper block inside XmRenderT.c (contract-backed shims)
sed -i '/^lib\/Xm\/XmRenderT\.c:/!b' "$tmp"
python3 - "$tmp" <<'PY2'
import sys
lines=[l for l in open(sys.argv[1]) if l.strip()]
def in_wrapper(num):
    return 2873 <= num <= 3095
out=[]
for l in lines:
    parts=l.split(':',2)
    if parts[0]=='lib/Xm/XmRenderT.c' and in_wrapper(int(parts[1])):
        continue
    out.append(l)
open(sys.argv[1],'w').writelines(out)
PY2
python3 - "$tmp" <<'PYEOF'
import sys, re

# Build an in-comment map per file so multi-line /* ... */ continuation
# lines are not flagged.
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
        # calls to the (allowed) legacy Xft wrappers
        if re.search(r'\b_XmXftDraw(String2|String|Create|Destroy|SetClipRectangles)?\s*\(', code):
            continue
        if 'TextListToTextProperty' in code or 'TextPropertyToList' in code:
            continue
        hits.append(f"{f}:{num}:{code}")
if hits:
    print("Phase-2 gate FAILED: direct X11 font/text calls outside XmPlat/exempts:")
    for h in hits: print(h)
    sys.exit(1)
print("Phase-2 gate OK: 0 violations")
PYEOF
rc=$?
rm -f "$tmp"
exit $rc