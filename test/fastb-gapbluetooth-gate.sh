#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Bluetooth: Radio class, IBluetoothAdapter2/3, GetRadioAsync (patches 3666): runs windows.devices.bluetooth_test.exe from the build tree
#
#   WINE=/path/to/build/wine test/fastb-gapbluetooth-gate.sh      (needs dlls/windows.devices.bluetooth/tests built beside it)
#   SG_XVFB_N=260 to name the display
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
BUILD=$(dirname "$WINE")
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
TX="$BUILD/dlls/windows.devices.bluetooth/tests/x86_64-windows/windows.devices.bluetooth_test.exe"
[ -f "$TX" ] || { echo "SKIP: tests not built in $BUILD"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-gapbluetooth.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOT
#!/bin/sh
timeout -s KILL 600 "$WINE" "$TX"  2>&1 </dev/null | tr -d '\r' | grep -E "tests executed|Test failed" > "$T/out"
EOT
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 700 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/out"
if grep "Test failed" "$T/out" | grep -qv '(NEVER)'; then exit 1; fi
grep -q "tests executed" "$T/out" || exit 1
echo "RESULT: PASS"
exit 0
