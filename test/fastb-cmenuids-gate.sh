#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Patch 3605: every item of the shell context menus (also the View, Arrange Icons and Send to submenu items)
# has a command ID in the range the menu returned (shell32's own shlfolder test, test_contextmenu).
#   WINE=/path/to/build/wine test/fastb-cmenuids-gate.sh      SG_XVFB_N=261 to name the display
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
BUILD=$(dirname "$WINE")
EXE="$BUILD/dlls/shell32/tests/x86_64-windows/shell32_test.exe"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] && [ -f "$EXE" ] || { echo "SKIP: needs a build tree with the shell32 tests"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-cmenuids.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$(dirname "$EXE")"
timeout -s KILL 300 "$WINE" ./shell32_test.exe shlfolder 2>&1 </dev/null | tr -d '\r' > "$T/out.txt"
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 400 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
grep -c "Test failed" "$T/out.txt" | sed 's/^/failures: /'
if grep -q "tests executed.* 0 failures" "$T/out.txt" && ! grep -q "Test failed" "$T/out.txt"; then
  echo "PASS  every context menu item has an ID in range"; exit 0
fi
echo "FAIL  shlfolder reported failures"; grep "Test failed" "$T/out.txt" | head; exit 1
