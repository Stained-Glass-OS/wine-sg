#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wininet: FindFirstUrlCacheEntryEx/FindNextUrlCacheEntryEx filter the cache entries by type.
#
#   WINE=/opt/wine-sg/bin/wine test/fastwininetenum-gate.sh   (mutants SG_MUTANT_WININET_NOFILTER)
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
T=$(mktemp -d /var/tmp/sg-fastwininetenum.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastwininetenum-probe.c" -lole32 -loleaut32 -luuid -lwininet || { fail "probe did not build"; exit 1; }
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
[ "$(grep -c "^ADD .* 1$" "$T/o")" = 3 ] && pass "three cache entries committed" || fail "entries not added: $(grep ADD "$T/o" | tr '\n' ' ')"
[ "$(w NORMAL_A)" = "a,b,c" ] && [ "$(w STICKY_A)" = "b" ] && [ "$(w EDITED_A)" = "c" ] && pass "the filter keeps the entries that have one of its type bits" || fail "filter: $(w NORMAL_A) / $(w STICKY_A)"
[ "$(w ALL_A)" = "a,b,c" ] && pass "filter 0 lists every entry" || fail "all: $(w ALL_A)"
[ "$(w NORMAL_W)" = "a,b,c" ] && [ "$(w STICKY_W)" = "b" ] && pass "the wide versions give the same" || fail "wide: $(w NORMAL_W) / $(w STICKY_W)"
[ "$(w GROUP)" = "0 2" ] && pass "a cache group is not found" || fail "group: $(w GROUP)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
