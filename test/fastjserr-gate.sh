#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# jscript: error codes of the parser and of thrown objects (Expected ';', prefix ++ after a line break, Error instances only).
#
#   WINE=/opt/wine-sg/bin/wine test/fastjserr-gate.sh   (mutants SG_MUTANT_JS_ANYOBJ_ERROR, SG_MUTANT_JS_SYNTAX_ONLY)
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
T=$(mktemp -d /var/tmp/sg-fastjserr.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastjserr-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
chk E1 "err 800a03ee 0 Expected ')'" "a -- after a line break is not a postfix operator: -->0 is a syntax error"
chk E2 "err 800a03ec 0 Expected ';'" "a byte order mark inside a word splits it: Expected ';'"
chk E3 "err 800a03ec 0 Expected ';'" "two statements on a line without a semicolon: Expected ';'"
chk E4 "ok 1,2" "++ at the start of a line starts a new statement"
chk E5 "ok 2,4" "a++ on one line is postfix; a line break before -- makes it a prefix of the next statement"
chk E6 "err 800a0009 1 test" "a thrown Error object gives its number and description and the user locale"
chk E7 "err 800a139e 0 Exception thrown and not caught" "a thrown plain object is not read for number or description"
chk E8 "err 80004005 1 " "a thrown Error without a number is E_FAIL with an empty description"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
