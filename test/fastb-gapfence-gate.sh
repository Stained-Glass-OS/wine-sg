#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# d3d11 fences (test_sg_fence in the d3d11 test; the known failures listed in the regex also fail on stock) (patches 3628): runs d3d11_test.exe from the build tree
#
#   WINE=/path/to/build/wine test/fastb-gapfence-gate.sh      (needs dlls/d3d11/tests built beside it)
#   SG_XVFB_N=260 to name the display
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
BUILD=$(dirname "$WINE")
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
TX="$BUILD/dlls/d3d11/tests/x86_64-windows/d3d11_test.exe"
[ -f "$TX" ] || { echo "SKIP: tests not built in $BUILD"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-gapfence.XXXXXX)
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
# The known stock failures (llvmpipe) by line: 3629 removed two todo blocks of 6 lines before them,
# so with 3629 applied (integ-2026-10) they are 6 lines earlier; both sets are listed. The failure set is
# the same as origin/main's d3d11 run (compared at integration).
if grep "Test failed" "$T/out" | grep -Evq 'd3d11.c:(20691|28875|28929|28938|20685|28869|28923|28932|6263|6285):'; then exit 1; fi
grep -q "tests executed" "$T/out" || exit 1
echo "RESULT: PASS"
exit 0
