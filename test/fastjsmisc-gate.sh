#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# jscript: Array.prototype.reduceRight and the ES6 Math functions (trunc, sign, cbrt, log2, ..., imul, clz32, fround).
#
#   WINE=/opt/wine-sg/bin/wine test/fastjsmisc-gate.sh   (mutants SG_MUTANT_JS_NOREDUCERIGHT, SG_MUTANT_JS_NOMATH)
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
T=$(mktemp -d /var/tmp/sg-fastjsmisc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastjsmisc-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
[ "$(v RR0)" = "4-3-2-1 321 1" ] && [ "$(v RR1)" = error ] && pass "reduceRight folds from the end, with and without a start value; an empty array without a start value fails" || fail "reduceRight: $(v RR0) $(v RR1)"
[ "$(v M0)" = "-4,4,-1,0,1,3,3,3,5,0,-60,31,32,5.5" ] && pass "Math.trunc, sign, cbrt, log2, log10, hypot, imul, clz32, fround" || fail "Math: $(v M0)"
[ "$(v M1)" = "0,0,1,0,0,0,0,0,true,true,0,Infinity" ] && pass "Math.expm1, log1p, cosh, sinh, tanh, acosh, asinh, atanh and the edge cases" || fail "Math2: $(v M1)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
