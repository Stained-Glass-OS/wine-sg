#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: IMarkupPointer2::MoveUnit by word end, sentence and address, and IsInsideURL.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlpointer-gate.sh   (mutants SG_MUTANT_MP_NOURL, SG_MUTANT_MP_NOINSIDE, SG_MUTANT_RANGE_NOSENTENCE)
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
T=$(mktemp -d /var/tmp/sg-fastmshtmlpointer.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmlpointer-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
[ "$(w UBEG1)" = "00000000 [http://a.exam]" ] && [ "$(w UBEG2)" = "00000000 [www.b.org now]" ] && pass "MoveUnit(NEXTURLBEG) goes to the next address" || fail "next url: $(w UBEG1) / $(w UBEG2)"
[ "$(w UEND1)" = "00000000 [, then www.b.]" ] && [ "$(w UEND2)" = "00000000 [ now. Done! O]" ] && pass "MoveUnit(NEXTURLEND) stops before the punctuation after an address" || fail "url end: $(w UEND1) / $(w UEND2)"
[ "$(w UPBEG2)" = "00000000 [www.b.org now]" ] && [ "$(w UPEND1)" = "00000000 [, then www.b.]" ] && [ "$(w UPBEG1)" = "00000000 [http://a.exam]" ] && [ "$(w UPBEG0)" = "00000000 [http://a.exam]" ] && pass "MoveUnit(PREVURLBEG/PREVURLEND) walk back over the addresses and stay at the first" || fail "prev url: $(w UPBEG2) $(w UPEND1) $(w UPBEG1) $(w UPBEG0)"
[ "$(w INURL0)" = "00000000 0" ] && [ "$(w INURL1)" = "00000000 1" ] && [ "$(w INURL2)" = "00000000 1" ] && [ "$(w INURL3)" = "00000000 0" ] && pass "IsInsideURL: plain text no, inside an address yes, spanning past the address no" || fail "IsInsideURL: $(w INURL0) $(w INURL1) $(w INURL2) $(w INURL3)"
[ "$(w WE1)" = "00000000 [ to http://a.]" ] && [ "$(w WE2)" = "00000000 [ http://a.exa]" ] && [ "$(w PWE1)" = "00000000 [ to http://a.]" ] && pass "MoveUnit by word end, forward and back" || fail "word end: $(w WE1) $(w WE2) $(w PWE1)"
[ "$(w S1)" = "00000000 [Done! Or not?]" ] && [ "$(w S2)" = "00000000 [Or not? Last ]" ] && [ "$(w PS1)" = "00000000 [Done! Or not?]" ] && [ "$(w PS0)" = "00000000 [go to http://]" ] && pass "MoveUnit by sentence, forward and back" || fail "sentence: $(w S1) $(w S2) $(w PS1) $(w PS0)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
