#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ieframe: WebBrowser progress, title, status text, secure lock icon and file download events
# (Wine's own ieframe webbrowser test, which has these checks, run from a build tree).
#
#   WINE=/path/obj/wine test/fastieframe-events-gate.sh
#   IEFRAME_TEST=... when the test exe is not at obj/dlls/ieframe/tests/x86_64-windows/ieframe_test.exe
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
TESTEXE="${IEFRAME_TEST:-$(dirname "$WINE")/dlls/ieframe/tests/x86_64-windows/ieframe_test.exe}"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f "$TESTEXE" ] || { echo "SKIP: no ieframe test exe at $TESTEXE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastieframe-events.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$T"
timeout -s KILL 400 "$WINE" "$TESTEXE" webbrowser > "$T/test.out" 2>&1 </dev/null
echo "exit \$?" >> "$T/test.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 500 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
grep -a "tests executed\|^exit" "$T/test.out"
grep -aq "tests executed (.*, 0 failures)" "$T/test.out" && ! grep -aq "Test failed" "$T/test.out" && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"
exit 1
