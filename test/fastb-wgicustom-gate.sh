#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Patch 3611: windows.gaming.input custom controller factories (dinput hotplug test): a factory that refuses a controller no longer crashes
# the manager (garbage inner pointer), weak references, IHidGameControllerProvider, OnInputResumed/Suspended around OnGameControllerAdded/Removed.
#   WINE=/path/to/build/wine test/fastb-wgicustom-gate.sh      SG_XVFB_N=261 to name the display
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
BUILD=$(dirname "$WINE")
EXE="$BUILD/dlls/dinput/tests/x86_64-windows/dinput_test.exe"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] && [ -f "$EXE" ] || { echo "SKIP: needs a build tree with the dinput tests"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-wgicustom.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$(dirname "$EXE")"
timeout -s KILL 300 "$WINE" ./dinput_test.exe hotplug 2>&1 </dev/null | tr -d '\r' > "$T/out.txt"
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 400 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
nfail=$(grep -c "Test failed\|Test succeeded inside todo" "$T/out.txt")
echo "failures: $nfail"
if [ "$nfail" = 0 ] && grep -q "hotplug: .* tests executed" "$T/out.txt"; then
  echo "PASS  custom controller factory path of the dinput hotplug test"; exit 0
fi
echo "FAIL  hotplug reported failures"; grep "Test failed\|succeeded inside" "$T/out.txt" | head; exit 1
