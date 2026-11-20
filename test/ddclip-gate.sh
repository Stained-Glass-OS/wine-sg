#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ddraw: a clipped Blt to the primary surface for a window partly off the screen (patches/sg/8408).
# Wine does not clip a window's visible region to the screen, so a clipper's list for a window
# hanging past an edge reached past the primary surface and the Blt failed (DDERR_INVALIDRECT),
# drawing nothing. Wine's VMR-7 blits every video frame that way: GraphStudioNext's video window,
# the film's own 1280x720 plus its frame, is wider than a 1280 screen and stayed white.
# test/ddclip-probe.c on a private Xvfb (1024x768), no window manager:
#   - the Blt for a window wholly on the screen succeeds (as before);
#   - the Blt for a window hanging 120 pixels past the right edge succeeds;
#   - its visible part shows the red frame (read off the screen).
#   WINE=/opt/wine-sg/bin/wine test/ddclip-gate.sh
# Mutant (ddraw): SG_MUTANT_DDRAW_CLIP_PAST_SURFACE (surface.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in "$MINGW" import Xvfb; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ddclip.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/ddclip-probe.exe" "$HERE/ddclip-probe.c" -lddraw -ldxguid -lgdi32 -luser32 ||
    { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
unset WAYLAND_DISPLAY

timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
timeout -s KILL 120 "$WINE" "$T/ddclip-probe.exe" >"$T/out.raw" 2>/dev/null </dev/null &
pp=$!
i=0; while ! grep -q '^pixel=' "$T/out.raw" 2>/dev/null && [ $i -lt 300 ]; do sleep 0.2; i=$((i + 1)); done
sleep 1
[ -n "${DDCLIP_SHOT:-}" ] && import -window root "$DDCLIP_SHOT"
px=$(tr -d '\r' < "$T/out.raw" | sed -n 's/^pixel=\([0-9]*\),\([0-9]*\)$/\1+\2/p')
rgb=
[ -n "$px" ] && rgb=$(import -window root -crop "1x1+$px" +repage -depth 8 rgb:- 2>/dev/null | od -An -tu1 | tr -s ' ' | sed 's/^ //')
wait $pp
tr -d '\r' < "$T/out.raw" > "$T/out"
grep -E '^(screen|client|on|off):' "$T/out" | sed 's/^/INFO  /'

fail=0
check() {
    if grep -qx "$1=1" "$T/out"; then echo "PASS  $2"; else echo "FAIL  $2"; fail=1; fi
}
check ddraw "DirectDraw, a primary surface and a source surface"
check on "the clipped Blt for a window wholly on the screen succeeds"
check off "the clipped Blt for a window hanging past the screen's edge succeeds (not DDERR_INVALIDRECT)"
set -- $rgb
if [ $# -eq 3 ] && [ "$1" -ge 200 ] && [ "$2" -le 60 ] && [ "$3" -le 60 ]; then
    echo "PASS  its visible part shows the frame (red at $px)"
else
    echo "FAIL  its visible part does not show the frame (at ${px:-?}: ${rgb:-nothing read})"; fail=1
fi
check done "the probe ran to the end"
[ $fail = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
