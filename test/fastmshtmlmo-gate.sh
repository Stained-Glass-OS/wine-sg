#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: MutationObserver (childList, attributes, characterData, subtree, filters, old values, takeRecords, disconnect).
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlmo-gate.sh   (mutants SG_MUTANT_MO_NOATTR, SG_MUTANT_MO_NOSUBTREE)
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
T=$(mktemp -d /var/tmp/sg-fastmshtmlmo.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmlmo-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
[ "$(v SYNC)" = 0 ] && [ "$(v ASYNC)" = 1 ] && pass "the callback runs after the script, once for the changes made in it" || fail "timing: $(v SYNC) $(v ASYNC)"
[ "$(v O1_0)" = "childList:a:1:0:null:null" ] && [ "$(v O1_2)" = "childList:a:0:1:null:null" ] && [ "$(v O1_SELF)" = true ] && pass "childList records for an added and a removed child; the observer is the second argument" || fail "childList: $(grep '^O1_' "$T/o" | tr '\n' ' ')"
[ "$(v O1_1)" = "attributes:a:0:0:title:t1" ] && pass "attributes record with the old value" || fail "attributes: $(v O1_1)"
[ "$(v O2_0)" = "attributes:p1:0:0:class:null" ] && [ -z "$(v O2_1)" ] && pass "subtree observation of a descendant and attributeFilter" || fail "subtree/filter: $(v O2_0) / $(v O2_1)"
[ "$(v O3_0)" = "characterData:undefined:0:0:null:abc:xyz" ] && pass "characterData record with the old value" || fail "characterData: $(v O3_0)"
[ "$(v TAKE0)" = "1 childList I true 0" ] && [ -z "$(v O4_CALLED)" ] && pass "takeRecords returns and clears the queue; disconnect drops what was left" || fail "takeRecords: $(v TAKE0) $(v O4_CALLED)"
[ "$(v O5_2)" = "childList:d:1:0:null:null" ] && pass "inserted nodes come as added nodes of a childList record" || fail "innerHTML: $(grep '^O5_' "$T/o" | tr '\n' ' ')"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
