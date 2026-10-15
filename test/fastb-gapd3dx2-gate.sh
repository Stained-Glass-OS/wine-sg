#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# d3dx9_36 gap batch 2 (teapot, animation compress, effect disassembly, env map projections, palettized save, L6V5U5/XR_BIAS formats, shader fragments) (patch 3641): runs d3dx9_36_test.exe from the build tree
#
#   WINE=/path/to/build/wine test/fastb-gapd3dx2-gate.sh      (needs dlls/d3dx9_36/tests built beside it)
#   SG_XVFB_N=260 to name the display
set -u
TESTS="${SG_TESTS:-mesh surface core effect texture shader}"
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
BUILD=$(dirname "$WINE")
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
TX="$BUILD/dlls/d3dx9_36/tests/x86_64-windows/d3dx9_36_test.exe"
[ -f "$TX" ] || { echo "SKIP: tests not built in $BUILD"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-gapd3dx2.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOT
#!/bin/sh
for s in $TESTS; do
timeout -s KILL 600 "$WINE" "$TX" \$s 2>&1 </dev/null | tr -d '\r' | grep -E "tests executed|Test failed"
done > "$T/out"
EOT
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 1600 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/out"
if grep "Test failed" "$T/out" | grep -qv 'NEVERMATCHES_xyz'; then exit 1; fi
[ "$(grep -c "tests executed" "$T/out")" = "$(echo $TESTS | wc -w)" ] || exit 1
echo "RESULT: PASS"
exit 0
