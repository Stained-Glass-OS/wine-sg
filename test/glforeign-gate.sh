#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct3D 11 through wined3d's OpenGL path presenting into another process's top-level window (6099), as
# Chromium's and Electron's GPU process does into the browser's window on machines without Vulkan. When
# the window needed its frames rendered offscreen (it has children and something clipped it when the
# drawable was made -- here, part of it is off the screen), the offscreen window stayed inside the
# browser's X window and the copies went astray: Slack 4.52's window stayed white. A foreign top-level now
# gets its client window in place (as 0586 for Vulkan), and the window's thread is told (6500's
# WM_WINE_CLIENT_SURFACE) so its own repaint leaves the frames alone.
# The host process shows a white window (framed, then a bare popup) with a child at -50,200, part of it under another window; the
# render process presents three red frames into it and stops; the host then redraws and erases itself.
#   WINE=/opt/wine-sg/bin/wine test/glforeign-gate.sh   (SG_XDISPLAY=N to use Xvfb :N)
# Mutants: SG_MUTANT_GL_FOREIGN_OFFSCREEN, SG_MUTANT_GL_FOREIGN_PAINTED (winex11 opengl.c).
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
T=$(mktemp -d /var/tmp/sg-glforeign.XXXXXX)
# wined3d (OpenGL), not an installed DXVK
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=;d3d11,dxgi=b" WINESERVER
unset DISPLAY XAUTHORITY
XP=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${ARTIFACTS:-}" ] && cp "$T"/*.png "$ARTIFACTS"/ 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -mwindows -o "$T/probe.exe" "$HERE/glforeign-probe.c" -ld3d11 -ldxgi -luuid -lgdi32 2>"$T/cc.log" ||
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
# STYLE: framed (the client area inside a frame: Slack's window) or popup (all of it client area)
check() {
    D="$WINEPREFIX/drive_c/gf-$1"; mkdir -p "$D"
    "$WINE" explorer /desktop=shell,1024x700 'C:\probe.exe' host "C:\\gf-$1" "$1" >/dev/null 2>&1 &
    i=0; while [ ! -s "$D/host.hwnd" ] && [ $i -lt 40 ]; do sleep 0.5; i=$((i + 1)); done
    "$WINE" explorer /desktop=shell,1024x700 'C:\probe.exe' render "C:\\gf-$1" >/dev/null 2>&1 &
    i=0; while ! grep -q presented=1 "$D/render.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
    tr -d '\r' < "$D/render.out" 2>/dev/null | sed "s/^/      $1: /"
    grep -q '^create=0' "$D/render.out" 2>/dev/null || { echo "SKIP: no Direct3D 11 through OpenGL here"; exit 77; }
    sleep 2
    import -window root "$T/$1-shown.png"
    p=$(px $1-shown 150 350); q=$(px $1-shown 50 480)
    [ "$p" = "255,0,0" ] && [ "$q" = "255,0,0" ] && pass "$1: the other process's frames are in the window ($p, $q)" \
        || fail "$1: the window shows $p / $q, not the red frames"
    : > "$D/redraw"
    i=0; while [ ! -e "$D/redrawn" ] && [ $i -lt 20 ]; do sleep 0.5; i=$((i + 1)); done
    sleep 2
    import -window root "$T/$1-redrawn.png"
    p=$(px $1-redrawn 150 350); q=$(px $1-redrawn 50 480)
    [ "$p" = "255,0,0" ] && [ "$q" = "255,0,0" ] && pass "$1: and stay when the window redraws and erases itself ($p, $q)" \
        || fail "$1: after the window's own repaint: $p / $q, not red"
    "$WINE" taskkill /im probe.exe /f >/dev/null 2>&1; sleep 2
}
check framed
check popup
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
