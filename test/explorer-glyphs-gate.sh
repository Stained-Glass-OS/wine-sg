#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# File Explorer's toolbar glyphs (patches/sg/0515), from a screenshot: the
# View grid's squares stand apart (the 1.25 pen ran them together into
# blots), and Refresh turns clockwise with its head at the leading end, the
# top (it sat at the arc's start, pointing back at the tail -- David,
# "the arrow points to the tail").
#
#   WINE=/opt/wine-sg/bin/wine test/explorer-glyphs-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb import python3; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: python3-pil missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-glyphs.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" explorer 'C:\users\Public' >/dev/null 2>&1 &
sleep 10
import -window root "$T/shot.png"
# the glyphs' cells, at the default window place on a 1280x800 screen
python3 - "$T/shot.png" > "$T/out" <<'PY'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('L')
def dark(x, y): return im.getpixel((x, y)) < 110
def box(x0, y0, w, h):
    pts = [(x, y) for x in range(x0, x0 + w) for y in range(y0, y0 + h) if dark(x, y)]
    if not pts: return None
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    return min(xs), min(ys), max(xs), max(ys)
b = box(421, 40, 30, 30)            # View
if b:
    cx = (b[0] + b[2]) // 2
    gap = sum(dark(cx, y) for y in range(b[1], b[3] + 1)) + sum(dark(cx + (b[2]-b[0]) % 2, y) for y in range(b[1], b[3] + 1))
    print("view_gap_dark", gap)
else:
    print("view_gap_dark none")
r = box(700, 86, 30, 30)            # Refresh
if r:
    cx = (r[0] + r[2]) // 2
    print("refresh_top_center", int(any(dark(x, y) for x in range(cx - 1, cx + 2) for y in range(r[1], r[1] + 3))))
else:
    print("refresh_top_center none")
PY
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }
[ "$(v view_gap_dark)" = 0 ] && pass "View: the grid's squares stand apart" || fail "View: squares run together ($(v view_gap_dark) dark px in the gap)"
[ "$(v refresh_top_center)" = 1 ] && pass "Refresh: clockwise, the head at the leading end (top)" || fail "Refresh: head at the tail ($(v refresh_top_center))"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
