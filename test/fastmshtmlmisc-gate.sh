#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: multi-column style, text range word moves, selection clear/empty, IOleObject::SetExtent.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlmisc-gate.sh   (mutants SG_MUTANT_MSHTML_NOMOZCOL, SG_MUTANT_MSHTML_NOWORDS)
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
T=$(mktemp -d /var/tmp/sg-fastmshtmlmisc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmlmisc-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
timeout -s KILL 120 "$WINE" "$T/probe.exe" >"$T/o" 2>/dev/null </dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
tr -d '\r' < "$T/o" > "$T/o2"; mv "$T/o2" "$T/o"
w() { sed -n "s/^$1 //p" "$T/o" | head -1; }
grep -q "^DONE" "$T/o" || { fail "the probe did not finish"; head -30 "$T/o"; echo "RESULT: FAIL"; exit 1; }
[ "$(sed -n 's/^COL0=//p' "$T/o")" = "3 20px" ] && pass "multi-column style properties reach the engine" || fail "columns"
[ "$(w MOVESTART)" = "00000000 1 [bb cc dd]" ] && [ "$(w MOVEEND)" = "00000000 -1 [bb cc ]" ] && pass "text range moveStart and moveEnd by words" || fail "word moves: $(w MOVESTART) / $(w MOVEEND)"
[ "$(w SELCLEAR)" = 00000000 ] && [ "$(w AFTERCLEAR)" = "[]" ] && [ "$(w SELEMPTY)" = 00000000 ] && [ "$(w SELTYPE)" = None ] && pass "selection.clear deletes the selected content; empty drops the selection" || fail "selection: $(grep '^SEL\|^AFTER' "$T/o" | tr '\n' ' ')"
[ "$(w EXT_BADBOTH)" = 80004005 ] && [ "$(w EXT_BADASPECT)" = 80070057 ] && [ "$(w EXT_NEG)" = 80004005 ] && [ "$(w EXT_OK)" = 00000000 ] && pass "SetExtent: a negative size fails first, then the aspect is checked" || fail "extent: $(grep '^EXT' "$T/o" | tr '\n' ' ')"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
