#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# jscript: Function.prototype.call/apply on a host function pass DISPATCH_METHOD only (checked by the conformance test run.c).
#
#   WINE=/path/obj/wine test/fastjscall-gate.sh   (mutant SG_MUTANT_JS_NOGET)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
OBJ="$(dirname "$WINE")"
TEST="${JSCRIPT_TEST:-$OBJ/dlls/jscript/tests/x86_64-windows/jscript_test.exe}"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f "$TEST" ] || { echo "SKIP: no jscript_test.exe at $TEST"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastjscall.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 300 "$WINE" "$TEST" run >"$T/o" 2>&1 </dev/null
tr -d '\r' < "$T/o" > "$T/o2"
if grep -q "wFlags" "$T/o2"; then
    echo "FAIL  call/apply on a host function still asks for a property value:"; grep wFlags "$T/o2" | head -3
    echo "RESULT: FAIL"; exit 1
fi
if grep -q "tests executed" "$T/o2"; then
    echo "PASS  Function.prototype.call/apply on host functions use DISPATCH_METHOD only"
    n=$(grep -c "Test failed" "$T/o2")
    [ "$n" = 0 ] && { echo "PASS  jscript run unit: no failures"; echo "RESULT: PASS"; exit 0; }
    echo "FAIL  $n failures"; echo "RESULT: FAIL"; exit 1
fi
echo "FAIL  the test did not finish"; tail -5 "$T/o2"; echo "RESULT: FAIL"; exit 1
