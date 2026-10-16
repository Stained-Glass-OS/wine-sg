#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: window.getSelection and document.getSelection give a Selection object; document.createRange is reachable from script; performance.now; getAttributeNode finds attributes that are only in the DOM.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlsel-gate.sh   (mutants SG_MUTANT_SEL_NOADD, SG_MUTANT_SEL_NORANGE, SG_MUTANT_PERF_NONOW, SG_MUTANT_ATTRNODE)
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
T=$(mktemp -d /var/tmp/sg-fastmshtmlsel.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmlsel-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
[ "$(v S0)" = "object 0 true 0" ] && [ "$(v S9)" = "[object Selection] true" ] && pass "window.getSelection and document.getSelection give a Selection with no range" || fail "object: $(v S0) $(v S9)"
[ "$(v SA)" = "object function function function" ] && [ "$(v SC)" = ok ] && pass "document.createRange gives a Range that addRange accepts" || fail "createRange: $(v SA) $(v SC)"
[ "$(v S1)" = "1 false [one] null 0 1" ] && pass "addRange: rangeCount, isCollapsed, toString, anchor and focus offsets" || fail "addRange: $(v S1)"
[ "$(v S2)" = "null one false" ] && pass "getRangeAt returns the range" || fail "getRangeAt: $(v S2)"
[ "$(v S3)" = "true 1 []" ] && [ "$(v S3b)" = 1 ] && pass "collapseToEnd and collapseToStart" || fail "collapse: $(v S3) $(v S3b)"
[ "$(v S4)" = "0 null" ] && pass "removeAllRanges" || fail "removeAllRanges: $(v S4)"
[ "$(v S5)" = "1 [x]" ] && [ "$(v S5b)" = "true b" ] && pass "selectAllChildren and collapse(node, offset)" || fail "select/collapse: $(v S5) $(v S5b)"
[ "$(v S6)" = "0 true" ] && [ "$(v S7)" = error ] && pass "deleteFromDocument; getRangeAt out of range fails" || fail "delete: $(v S6) $(v S7)"
[ "$(v S8a)" = 1 ] && [ "$(v S8)" = 0 ] && pass "removeRange" || fail "removeRange: $(v S8a) $(v S8)"
[ "$(v P0)" = "number true true true true" ] && pass "performance.now gives increasing milliseconds with a fraction" || fail "performance.now: $(v P0)"
[ "$(v GA0)" = "object false" ] && [ "$(v GA1)" = "t1 title true t1" ] && [ "$(v GD)" = "5 data-x" ] && [ "$(v GD2)" = a ] && [ "$(v GC)" = true ] && pass "getAttributeNode gives the attribute node of a markup or setAttribute attribute, null for none" || fail "getAttributeNode: $(v GA0) $(v GA1) $(v GD) $(v GD2) $(v GC)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
