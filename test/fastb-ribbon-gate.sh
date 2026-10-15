#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# UI ribbon framework object model and image factory (patch 3579)
#
#   WINE=/opt/wine-sg/bin/wine test/fastb-ribbon-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
#   SG_XVFB_N=260 to name the display
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-ribbon.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$T/inc"
BUILD=$(dirname "$WINE")
[ -f "$BUILD/include/uiribbon.h" ] || { echo "SKIP: needs $BUILD/include/uiribbon.h"; exit 77; }
cp "$BUILD/include/uiribbon.h" "$T/inc/"
printf 'ribbon markup stand-in' > "$T/dummy.bin"
echo 'APPLICATION_RIBBON UIRIBBON "dummy.bin"' > "$T/res.rc"
(cd "$T" && x86_64-w64-mingw32-windres res.rc res.o) || { echo "FAIL  windres"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastb-ribbon-probe.c" -I"$T/inc" "$T/res.o" -lole32 -loleaut32 -luuid -luser32 -lgdi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 300 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
