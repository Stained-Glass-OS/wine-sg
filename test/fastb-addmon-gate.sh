#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# localspl (patch 3573): AddMonitor + DeleteMonitor unload a resident monitor, so a later AddMonitor of a missing DLL fails.
# Runs Wine's winspool.drv conformance test (info) from the build tree: it must have 0 failures.
#   WINE=/path/to/build/wine test/fastb-addmon-gate.sh      SG_XVFB_N=261 to name the display
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
BUILD=$(dirname "$WINE")
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T64="$BUILD/dlls/winspool.drv/tests/x86_64-windows/winspool.drv_test.exe"
[ -f "$T64" ] || { echo "SKIP: winspool.drv_test.exe not built"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-addmon.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$(dirname "$T64")"
timeout -s KILL 300 "$WINE" ./winspool.drv_test.exe info 2>&1 </dev/null | tr -d '\r' > "$T/test.out"
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 400 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
grep -E "Test failed|tests executed" "$T/test.out"
grep -q "Test failed" "$T/test.out" && { echo "RESULT: FAIL"; exit 1; }
grep -q "0 failures" "$T/test.out" || { echo "RESULT: FAIL (no summary)"; exit 1; }
echo "RESULT: PASS"
