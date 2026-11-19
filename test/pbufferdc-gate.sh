#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SetPixelFormat on a pbuffer's DC (patches/sg/7905). Java 3D (Sweet Home 3D's 3D view) takes a
# pbuffer's DC with wglGetPbufferDCARB, sets the pbuffer's own pixel format on it and makes its
# offscreen context there. winex11 refused any DC that is not a window's ("not a valid window
# DC"), Java 3D reported "Failed in SetPixelFormat" and Sweet Home 3D closed with a fatal 3D
# error. As on Windows, the pbuffer's own format is accepted (a format already set is set again),
# any other refused; a context made on that DC can be made current. test/pbufferdc-probe.c, 64-bit,
# on an Xvfb of its own (Mesa).
#   WINE=/opt/wine-sg/bin/wine test/pbufferdc-gate.sh
# Mutant: -DSG_MUTANT_PBUFFER_DC_NO_FORMAT (dlls/winex11.drv/opengl.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc Xvfb; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pbufferdc.XXXXXX); XP=
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/pbufferdc-probe.c" -lopengl32 -lgdi32 -luser32 ||
    { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
if ! printf '%s\n' "$out" | grep -qx 'pbuffer=1'; then echo "SKIP: no OpenGL pbuffers on this display"; exit 77; fi
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1$"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep -o "^$1=[^ ]*" | head -1 || echo none))"; fi
}
check getformat   "GetPixelFormat on the pbuffer's DC is the pbuffer's format"
check sameformat  "SetPixelFormat with that format succeeds on the pbuffer's DC"
check otherformat "SetPixelFormat with another format fails"
check stillformat "the pbuffer keeps its format"
check context     "a context made on the pbuffer's DC is made current"
check screendc    "the screen's DC still refuses a format"
check done        "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
