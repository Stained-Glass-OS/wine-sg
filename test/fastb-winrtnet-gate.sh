#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# windows.networking.hostname / .connectivity / geolocation (patches 3552, 3553): runs the DLLs' conformance tests from a
# build tree (the HostName and Geolocator properties are checked there).
#
#   WINE=/path/to/build/wine test/fastb-winrtnet-gate.sh      (needs dlls/*/tests built beside it)
#   SG_XVFB_N=260 to name the display
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
BUILD=$(dirname "$WINE")
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
HT="$BUILD/dlls/windows.networking.hostname/tests/x86_64-windows/windows.networking.hostname_test.exe"
CT="$BUILD/dlls/windows.networking.connectivity/tests/x86_64-windows/windows.networking.connectivity_test.exe"
GT="$BUILD/dlls/geolocation/tests/x86_64-windows/geolocation_test.exe"
[ -f "$HT" ] || { echo "SKIP: tests not built in $BUILD"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-winrtnet.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
for t in "$HT" "$CT" "$GT"; do
  [ -f "\$t" ] || continue
  timeout -s KILL 120 "$WINE" "\$t" 2>&1 </dev/null | tr -d '\r' | grep -E "tests executed|Test failed" 
done > "$T/out"
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 400 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/out"
grep -q "Test failed" "$T/out" && exit 1
grep -q "tests executed (.* 0 failures" "$T/out" || exit 1
echo "RESULT: PASS"
exit 0
