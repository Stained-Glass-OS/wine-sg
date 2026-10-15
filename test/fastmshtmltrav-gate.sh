#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: document.createNodeIterator and createTreeWalker (filters, whatToShow, live removal).
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmltrav-gate.sh   (mutants SG_MUTANT_TRAV_NOFILTER)
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
T=$(mktemp -d /var/tmp/sg-fastmshtmltrav.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmltrav-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
[ "$(v IT0)" = "object a 1 true a,p1,b1,p2" ] && [ "$(v IT1)" = "a,p1,#text,b1,#text,p2,#text,#comment" ] && pass "createNodeIterator: root, whatToShow, no filter, element and all-node walks" || fail "iterator: $(v IT0) / $(v IT1)"
[ "$(v IT2)" = "a,b1 seen=a,p1,b1,p2 function" ] && [ "$(v IT3)" = "a,p1,p2" ] && pass "a filter function (skip) and a filter object with acceptNode (reject) are called" || fail "filter: $(v IT2) / $(v IT3)"
[ "$(v IT4)" = "a p1 p1" ] && [ "$(v IT5)" = p2 ] && pass "previousNode, detach, and the iterator follows the removal of a node" || fail "iterator moves: $(v IT4) $(v IT5)"
[ "$(v TW0)" = "a a p1 p2 null a p2" ] && pass "TreeWalker: root, currentNode, firstChild, nextSibling, parentNode, lastChild" || fail "walker: $(v TW0)"
[ "$(v TW1)" = "p1,b1,p2 p2" ] && [ "$(v TW1b)" = "p1 b1 p1 null" ] && pass "TreeWalker: nextNode walk, currentNode can be set, previousNode and previousSibling" || fail "walker moves: $(v TW1) $(v TW1b)"
[ "$(v TW2)" = "p1,p2" ] && [ "$(v TW3)" = "x,y,z" ] && pass "TreeWalker filter reject and text-only whatToShow" || fail "walker filter: $(v TW2) $(v TW3)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
