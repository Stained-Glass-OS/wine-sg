#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# userconsentverifier factory answers IAgileObject (patch 3733): runs the Wine test from the build tree
#
#   WINE=/path/to/build/wine test/fastb-ucvagile-gate.sh      (needs dlls/windows.security.credentials.ui.userconsentverifier/tests built beside it)
#   SG_XVFB_N=260 to name the display
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
BUILD=$(dirname "$WINE")
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
TX="$BUILD/dlls/windows.security.credentials.ui.userconsentverifier/tests/x86_64-windows/windows.security.credentials.ui.userconsentverifier_test.exe"
[ -f "$TX" ] || { echo "SKIP: tests not built in $BUILD"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-ucvagile.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOT
#!/bin/sh
timeout -s KILL 1500 "$WINE" "$TX"  2>&1 </dev/null | tr -d '\r' | grep -E "tests executed|Test failed" > "$T/out"
EOT
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 1600 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/out"
if grep -q "Test failed" "$T/out"; then exit 1; fi
grep -q "tests executed" "$T/out" || exit 1
echo "RESULT: PASS"
exit 0
