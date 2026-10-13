#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ieframe: the window object of a browser (SID_SHTMLWindow) forwards to the page window; connection point enumerators.
#
#   WINE=/opt/wine-sg/bin/wine test/fastieframe-gate.sh   (mutants SG_MUTANT_IEFRAME_NOFWD, SG_MUTANT_IEFRAME_NOCPENUM)
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
T=$(mktemp -d /var/tmp/sg-fastieframe.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastieframe-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
timeout -s KILL 150 "$WINE" "$T/probe.exe" >"$T/o" 2>/dev/null </dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
tr -d '\r' < "$T/o" > "$T/o2"; mv "$T/o2" "$T/o"
w() { sed -n "s/^$1 //p" "$T/o" | head -1; }
grep -q "^DONE" "$T/o" || { fail "the probe did not finish"; head -30 "$T/o"; echo "RESULT: FAIL"; exit 1; }
[ "$(w READY)" = 4 ] && [ "$(w SERVICE)" = "00000000 1" ] && pass "a browser navigates to a local page and hands out its window (SID_SHTMLWindow)" || fail "setup: $(w READY) $(w SERVICE)"
[ "$(w LENGTH)" = "00000000 0" ] && [ "$(w PUTNAME)" = 00000000 ] && [ "$(w GETNAME)" = "00000000 sgwin" ] && pass "length, put_name and get_name reach the page window" || fail "length/name: $(w LENGTH) $(w PUTNAME) $(w GETNAME)"
[ "$(w LOCATION)" = "00000000 file:" ] && [ "$(w NAVIGATOR)" = "00000000 1" ] && [ "$(w SCREEN)" = "00000000 1" ] && pass "location, navigator and screen" || fail "objects: $(w LOCATION) $(w NAVIGATOR) $(w SCREEN)"
[ "$(w EXEC)" = 00000000 ] && [ "$(w TITLE)" = "00000000 viaie" ] && pass "execScript runs in the page; get_document sees the change" || fail "execScript: $(w EXEC) $(w TITLE)"
[ "$(w SCROLL)" = 00000000 ] && [ "$(w TOSTRING)" = "00000000 1" ] && [ "$(w DEFSTATUS)" = 00000000 ] && [ "$(w CLOSED)" = "00000000 0" ] && pass "scrollTo, toString, defaultStatus, closed" || fail "misc: $(w SCROLL) $(w TOSTRING) $(w DEFSTATUS) $(w CLOSED)"
[ "$(w ENUMCP)" = 00000000 ] && [ "$(w CPCOUNT)" = 3 ] && [ "$(w CPSKIP)" = 00000000 ] && [ "$(w CPCLONE)" = 00000000 ] && [ "$(w CPREST)" = 2 ] && pass "EnumConnectionPoints lists three points; Reset, Skip and Clone work" || fail "cp enum: $(grep '^CP\|^ENUMCP' "$T/o" | tr '\n' ' ')"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
