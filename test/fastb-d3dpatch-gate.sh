#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# d3d8 and d3d9 (patch 3681): DrawRectPatch, DrawTriPatch, DeletePatch. test/fastb-d3d8patch-probe.c and fastb-d3d9patch-probe.c.
#
#   WINE=/opt/wine-sg/bin/wine test/fastb-d3drm-gate.sh
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
T=$(mktemp -d /var/tmp/sg-fastb-d3dpatch.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for n in 8 9; do
    TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe$n.exe" "$HERE/fastb-d3d${n}patch-probe.c" \
        || { echo "FAIL  probe $n did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe8.exe" "$T/probe9.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
for n in 9 8; do
    timeout -s KILL 180 "$WINE" probe\$n.exe 2>/dev/null </dev/null | tr -d '\r' >> "$T/probe.out"
done
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 300 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
[ "$(grep -cx 'RESULT: PASS' "$T/probe.out")" = 2 ] && exit 0
exit 1
