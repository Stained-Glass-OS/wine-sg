#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# opengl32 wow64 pointer getters (glGetPointerv & co., GL_EXT_direct_state_access getters) and withheld extensions (patch 3635)
# Runs the same probe as a 64-bit and a 32-bit (wow64) program.
#
#   WINE=/path/to/build/wine test/fastb-gapogl-gate.sh
#   SG_XVFB_N=260 to name the display
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: i686 mingw not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-gapogl.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe64.exe" "$HERE/fastb-gapogl-probe.c" -lopengl32 -lgdi32 -luser32 \
    || { echo "FAIL  64-bit probe did not build"; exit 1; }
TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/probe32.exe" "$HERE/fastb-gapogl-probe.c" -lopengl32 -lgdi32 -luser32 \
    || { echo "FAIL  32-bit probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
echo "== 64-bit"; timeout -s KILL 120 "$WINE" probe64.exe 2>/dev/null </dev/null | tr -d '\r'
echo "== 32-bit"; timeout -s KILL 120 "$WINE" probe32.exe 2>/dev/null </dev/null | tr -d '\r'
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 300 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh" > "$T/out"
cat "$T/out"
[ "$(grep -c '^RESULT: PASS' "$T/out")" = 2 ] && ! grep -q '^FAIL' "$T/out" && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"
exit 1
