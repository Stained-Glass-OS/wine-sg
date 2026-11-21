#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A pbuffer's front buffer with a software renderer (patches/sg/8412). Java 3D (Sweet Home 3D's 3D
# view) clears a 1x1 pbuffer with glDrawBuffer(GL_FRONT) and glFinish; with Mesa's software renderer
# the image went to the GLXPbuffer with XShmPutImage, the X server answered BadMatch and the X error
# ended the program before any window. winex11 now gives such pbuffers a GLX pixmap. 64- and 32-bit,
# test/pbufferfront-probe.c, each twice (create, draw, read back, destroy), on an Xvfb of its own.
#   WINE=/opt/wine-sg/bin/wine test/pbufferfront-gate.sh
# Mutant: -DSG_MUTANT_PBUFFER_NO_PIXMAP (dlls/winex11.drv/opengl.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc Xvfb; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pbufferfront.XXXXXX); XP=
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/pbufferfront-probe.c" -lopengl32 -lgdi32 -luser32 ||
        { fail "$a probe did not build"; exit 1; }
done
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
for a in x86_64 i686; do
    cp "$T/probe-$a.exe" "$WINEPREFIX/drive_c/"
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" "C:\\probe-$a.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    if ! printf '%s\n' "$out" | grep -qx 'pbuffer=1'; then echo "SKIP: no OpenGL pbuffers on this display"; exit 77; fi
    [ "$(printf '%s\n' "$out" | grep -cx 'front=1')" = 2 ] && pass "$a: drawing to the front buffer and glFinish, twice, the program lives" ||
        fail "$a: the program died drawing to a pbuffer's front buffer"
    [ "$(printf '%s\n' "$out" | grep -c '^readback=1 ')" = 2 ] && pass "$a: the front buffer reads back what was drawn" ||
        fail "$a: the front buffer does not read back red ($(printf '%s\n' "$out" | grep '^readback' | head -1))"
    printf '%s\n' "$out" | grep -qx 'done=1' && pass "$a: pbuffers destroyed, the program ends normally" || fail "$a: did not end normally"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
