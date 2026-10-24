#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A child window's frame presented while its top-level is hidden (6096). Wine's built-in Direct3D draws
# through OpenGL; a child window's frames are rendered offscreen and copied into the top-level's X window.
# A frame presented while the top-level was hidden went into an unmapped X window, which keeps nothing:
# Electron presents its first frame before it shows a window (ready-to-show), and on machines without
# Vulkan (wined3d's OpenGL path) such a window stayed black until something in it moved. Windows'
# compositor keeps a swap chain's last frame. Now the frame is copied again when the top-level is exposed.
# The probe presents one red frame into a child window, hidden first, then (the control) shown first.
#   WINE=/opt/wine-sg/bin/wine test/glhiddenframe-gate.sh   (SG_XDISPLAY=N to use Xvfb :N)
# Mutant: SG_MUTANT_GL_NO_RESHOW (winex11 event.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=x86_64-w64-mingw32-gcc
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in Xvfb import convert $MINGW; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
T=$(mktemp -d /var/tmp/sg-glhiddenframe.XXXXXX)
# wined3d (OpenGL), not an installed DXVK
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=;d3d11,dxgi=b" WINESERVER
unset DISPLAY XAUTHORITY
XP=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${ARTIFACTS:-}" ] && cp "$T"/*.png "$ARTIFACTS"/ 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/glhiddenframe-probe.c" -ld3d11 -ldxgi -luuid -lgdi32 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
if [ -n "${SG_XDISPLAY:-}" ]; then
    Xvfb ":$SG_XDISPLAY" -screen 0 1024x700x24 -nolisten tcp +extension GLX >/dev/null 2>&1 & XP=$!
    echo "$SG_XDISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp +extension GLX 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
sleep 8
px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null | tail -n 1; }
run() {   # MODE: run the probe, take the screen, stop the probe
    "$WINE" explorer /desktop=shell,1024x700 'C:\probe.exe' "$1" "C:\\$1.out" >/dev/null 2>&1 &
    i=0; while ! grep -q present= "$WINEPREFIX/drive_c/$1.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
    sleep 4
    import -window root "$T/$1.png"
    tr -d '\r' < "$WINEPREFIX/drive_c/$1.out" | grep -E '^(device|present)=' | sed "s/^/      $1: /"
    "$WINE" taskkill /im probe.exe /f >/dev/null 2>&1; sleep 2
}
run shown
tr -d '\r' < "$WINEPREFIX/drive_c/shown.out" | grep -q '^present=0$' || { echo "SKIP: no Direct3D 11 through OpenGL here"; exit 77; }
p=$(px shown 500 350)
[ "$p" = "255,0,0" ] || { echo "SKIP: a frame presented to a shown window does not reach the screen here ($p)"; exit 77; }
pass "control: a frame presented into a shown window is on screen ($p)"
run hidden
p=$(px hidden 500 350)
[ "$p" = "255,0,0" ] && pass "a frame presented while the window was hidden is on screen once it is shown ($p)" \
    || fail "the frame presented while hidden is lost: $p, not red"
p=$(px hidden 320 220); q=$(px hidden 690 490)
[ "$p" = "255,0,0" ] && [ "$q" = "255,0,0" ] && pass "all of it, corner to corner" || fail "corners: $p / $q"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
