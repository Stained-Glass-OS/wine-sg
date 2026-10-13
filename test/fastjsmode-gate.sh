#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# jscript/mshtml: typed arrays only from IE10 document mode; ES6 Math functions never in IE modes (engine flag 0x104 only).
#
#   WINE=/opt/wine-sg/bin/wine test/fastjsmode-gate.sh   (mutant SG_MUTANT_JS_TYPED_ALWAYS)
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
T=$(mktemp -d /var/tmp/sg-fastjsmode.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastjsmode-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
chk() { [ "$(v "$1")" = "$2" ] && pass "$3" || fail "$3: got '$(v "$1")'"; }
chk E_ES5 "function,function,function" "engine ES5 HTML: typed arrays present"
chk E_ES5_NOTYPED "undefined,undefined,undefined" "engine ES5 + NOTYPED: ArrayBuffer, Int8Array, DataView absent"
chk E_ES6 "function,function,function" "engine ES6: typed arrays present"
chk E_ES6_MATH "undefined,undefined" "engine ES6: no ES6 Math functions (IE11 has none)"
chk E_NEXT_MATH "-4,-1,3,3,5,0,31,5.5,0,1" "engine ESNEXT: ES6 Math functions work"
chk H9 "undefined,undefined,undefined,undefined,undefined" "IE9 document mode: no typed arrays"
chk H10 "function,function,function,function,undefined" "IE10 document mode: typed arrays, no Math.trunc"
chk H11 "function,function,function,function,undefined" "IE11 document mode: typed arrays, no Math.trunc"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
