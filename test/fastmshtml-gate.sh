#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: table sections (THEAD/TBODY/TFOOT), captions and TxtRange.findText.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtml-gate.sh   (mutant SG_MUTANT_MSHTML_FIND_ANYCASE)
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
T=$(mktemp -d /var/tmp/sg-fastmshtml.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtml-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
timeout -s KILL 120 "$WINE" "$T/probe.exe" >"$T/o" 2>/dev/null </dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
tr -d '\r' < "$T/o" > "$T/o2"; mv "$T/o2" "$T/o"
v() { sed -n "s/^$1 //p" "$T/o" | head -1; }
grep -q "^DONE" "$T/o" || { fail "the probe did not finish"; head -30 "$T/o"; echo "RESULT: FAIL"; exit 1; }
[ "$(v TBODY)" = "00000000 TBODY rows 2" ] && pass "a tbody is a table section with its rows" || fail "tbody: $(v TBODY)"
[ "$(v INSERT)" = "00000000 TR rows 3" ] && [ "$(v INSERT_BAD)" = 80070057 ] && pass "insertRow adds a TR at the index; past the end is refused" || fail "insert: $(v INSERT) $(v INSERT_BAD)"
[ "$(v DELETE)" = "00000000 rows 2" ] && [ "$(v DELETE_BAD)" = 80070057 ] && pass "deleteRow(-1) removes the last; a bad index is refused" || fail "delete: $(v DELETE) $(v DELETE_BAD)"
[ "$(v ALIGN)" = center ] && pass "the section's align is kept" || fail "align: $(v ALIGN)"
[ "$(v HEAD0)" = "00000000 0" ] && [ "$(v HEAD_CREATE)" = "00000000 THEAD" ] && [ "$(v HEAD1)" = "00000000 1" ] && [ "$(v HEAD2)" = "00000000 0" ] \
    && pass "tHead: none, createTHead makes it, deleteTHead takes it away" || fail "thead: $(grep '^HEAD' "$T/o" | tr '\n' ' ')"
[ "$(v FOOT_CREATE)" = "00000000 TFOOT" ] && [ "$(v FOOT1)" = "00000000 1" ] && [ "$(v FOOT2)" = "00000000 0" ] && pass "tFoot likewise" || fail "tfoot: $(grep '^FOOT' "$T/o" | tr '\n' ' ')"
[ "$(v CAPTION)" = "00000000 CAPTION" ] && [ "$(v CAPVALIGN)" = bottom ] && [ "$(v CAPTION2)" = "00000000 0" ] && pass "createCaption, vAlign, deleteCaption" || fail "caption: $(grep '^CAP' "$T/o" | tr '\n' ' ')"
[ "$(v F1)" = "1 Abc" ] && [ "$(v F2)" = "1 abc" ] && [ "$(v F3)" = "1 abc" ] && [ "$(v F4)" = "1 Abc" ] && [ "$(v F5)" = "0 []" ] \
    && pass "findText steps through the matches (any case), then fails and leaves the range" || fail "find: $(grep '^F[0-9]' "$T/o" | tr '\n' ' ')"
[ "$(v CASE)" = "1 abc | 0" ] && pass "match case (flag 4)" || fail "case: $(v CASE)"
[ "$(v WORD)" = "1 Abc 1 abc 1 Abc" ] && pass "whole words (flag 2) skip abc2" || fail "word: $(v WORD)"
[ "$(v COUNT)" = "0 1 xyz" ] && pass "count limits the characters searched" || fail "count: $(v COUNT)"
[ "$(v BACK)" = "1 Abc 1 abc" ] && pass "a negative count searches backwards" || fail "back: $(v BACK)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
