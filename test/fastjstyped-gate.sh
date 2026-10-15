#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# jscript: typed arrays (Int8Array .. Float64Array) over an ArrayBuffer.
#
#   WINE=/opt/wine-sg/bin/wine test/fastjstyped-gate.sh   (mutants SG_MUTANT_JS_TA_NOWRAP, SG_MUTANT_JS_TA_NOOOB)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-fastjstyped.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastjstyped-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
timeout -s KILL 120 "$WINE" "$T/probe.exe" >"$T/o" 2>/dev/null </dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
tr -d '\r' < "$T/o" > "$T/o2"; mv "$T/o2" "$T/o"
v() { sed -n "s/^$1=//p" "$T/o" | head -1; }
w() { sed -n "s/^$1 //p" "$T/o" | head -1; }
grep -q "^DONE" "$T/o" || { fail "the probe did not finish"; head -30 "$T/o"; echo "RESULT: FAIL"; exit 1; }
[ "$(v A0)" = "4 4 0 0 1 1 true 4" ] && pass "new Uint8Array(4): length, byteLength, byteOffset, BYTES_PER_ELEMENT, buffer" || fail "A0: $(v A0)"
[ "$(v A1)" = "1,255,3,7, 4" ] && pass "Uint8Array stores modulo 256, truncates, converts strings, ignores writes beyond the end" || fail "A1: $(v A1)"
[ "$(v A2)" = "255,0,2,2,0" ] && pass "Uint8ClampedArray clamps and rounds half to even" || fail "A2: $(v A2)"
[ "$(v A3)" = "127,-128,-1,127" ] && grep -q "^-32768,32767,-1$" "$T/o" && grep -q "^65535,0,1$" "$T/o" && pass "Int8, Int16 and Uint16 arrays wrap around" || fail "A3: $(v A3)"
[ "$(v A4)" = "-2147483648,-1,0,1,-1" ] && grep -q "^4294967295,0,1,2147483648$" "$T/o" && pass "Int32 and Uint32 arrays wrap around" || fail "A4: $(v A4)"
[ "$(v A5)" = "0.100000001490116 16777216 Infinity -Infinity" ] && grep -q "^0.1 0.333333333333333$" "$T/o" && pass "Float32Array rounds to single, Float64Array keeps doubles" || fail "A5: $(v A5)"
[ "$(v B0)" = "4,3,2,1 2 true" ] && [ "$(v B1)" = "2 2 4 258" ] && [ "$(v B2)" = "4 4" ] && pass "views of one ArrayBuffer share the bytes (little endian), with offset and length" || fail "B: $(v B0) / $(v B1) / $(v B2)"
[ "$(v C0)" = "3 1 2,3,4" ] && [ "$(v C1)" = "9 2 3 0" ] && pass "subarray shares the buffer; negative, open and reversed ranges" || fail "subarray: $(v C0) $(v C1)"
[ "$(v C2)" = "1,2,3,5,7,8" ] && [ "$(v C3)" = error ] && pass "set copies an array or typed array at an offset and refuses to overrun" || fail "set: $(v C2) $(v C3)"
[ "$(v D0)" = "[object Uint8Array] [object Float64Array] true true false" ] && pass "toString names the type; ArrayBuffer.isView knows typed arrays" || fail "toString: $(v D0)"
[ "$(v D1)" = "0,1,2 5,6,7 true true false" ] && [ "$(v D2)" = "2,3 2" ] && [ "$(v D3)" = "0 0" ] && pass "for-in lists the indices, slice.call works, instanceof and constructor, copy of another typed array, empty array" || fail "misc: $(v D1) $(v D2) $(v D3)"
[ "$(v E0)" = error ] && [ "$(v E1)" = error ] && [ "$(v E2)" = error ] && pass "a bad offset, a bad byte length and a negative length are refused" || fail "errors: $(v E0) $(v E1) $(v E2)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
