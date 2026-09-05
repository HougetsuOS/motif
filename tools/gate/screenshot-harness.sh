#!/bin/sh
# Screenshot harness (plan §3 Phase 0): run Motif demos under Xvfb and
# capture per-app PNGs. Baselines live in tests/screenshots/.
# Usage: tools/gate/screenshot-harness.sh [--update]
#   default: capture and diff against baseline (exit 1 on diff)
#   --update: (re)write baselines
set -e
cd "$(dirname "$0")/../.." || exit 1
MODE="$1"
BASE=tests/screenshots
TMP=$(mktemp -d /tmp/opencode/motif-shots.XXXXXX)
trap 'kill $XVFB_PID 2>/dev/null; rm -rf "$TMP"' EXIT
export PATH="$PATH"
XVFB_PID=""
start_xvfb() {
  Xvfb :97 -screen 0 1024x768x24 >/dev/null 2>&1 &
  XVFB_PID=$!
  export DISPLAY=:97
  for i in 1 2 3 4 5 6 7 8 9 10; do
    if xdpyinfo >/dev/null 2>&1; then return 0; fi
    sleep 0.5
  done
  echo "Xvfb failed to start" >&2; return 1
}
start_xvfb

shot() { # name command...
  name="$1"; shift
  "$@" &
  pid=$!
  sleep 3
  xwd -root -silent | xwdtopnm 2>/dev/null | pnmtopng > "$TMP/$name.png" 2>/dev/null || {
    echo "capture failed for $name" >&2; kill $pid 2>/dev/null; return 1; }
  kill $pid 2>/dev/null || true
  wait $pid 2>/dev/null || true
  if [ "$MODE" = "--update" ]; then
    mkdir -p "$BASE"; cp "$TMP/$name.png" "$BASE/$name.png"; echo "baseline updated: $name"
  elif [ -f "$BASE/$name.png" ]; then
    if cmp -s "$BASE/$name.png" "$TMP/$name.png"; then echo "match: $name"
    else echo "DIFF: $name"; exit 1; fi
  else
    echo "no baseline for $name (run with --update)" >&2; return 1
  fi
}

shot hellomotif sh -c 'cd demos/programs/hellomotif && ./hellomotif'
shot periodic   sh -c 'cd demos/programs/periodic && ./periodic'
echo "screenshots OK"
