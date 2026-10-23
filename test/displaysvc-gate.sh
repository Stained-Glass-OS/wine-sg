#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: IDisplayServices' CreateDisplayPointer, GetCaret and GetComputedStyle (6051) -- they gave
# E_NOTIMPL. HTML editors in older programs (DHTML edit hosts, mail composers) place display
# pointers by point and by line, move the caret to them and ask what the text there looks like.
# The document is shown in a window (ATL's AtlAxWin, "mshtml:" text) under Xvfb:
#  - a display pointer hit-tests a point to the character under it (the position after it past its
#    middle), with HT_RESULTS_Glyph; IsLeftOf, IsEqualTo;
#  - MoveUnit: the next line at the same x, the line's start (IsAtBOL there, not mid-line; a <br>
#    break reported by QueryBreaks), the line's end, the previous line at a given x; line info at
#    the line's start; the flow element (the paragraph);
#  - the caret moves to a display pointer, a markup pointer moves to it, GetLocation is its box,
#    InsertText inserts there and leaves the caret after; Hide;
#  - computed styles: bold, weight 700, underline, red, a face for <u> in a bold red paragraph;
#    plain text not bold, unlike it, like its paragraph.
#   WINE=/opt/wine-sg/bin/wine test/displaysvc-gate.sh   (mutants SG_MUTANT_DISPLAY_FIRST_CHAR, SG_MUTANT_CARET_NO_MOVE, SG_MUTANT_COMPSTYLE_DEFAULTS)
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
T=$(mktemp -d /var/tmp/sg-displaysvc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/displaysvc-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
timeout -s KILL 120 "$WINE" "$T/probe.exe" >"$T/o" 2>"$T/e" </dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
tr -d '\r' < "$T/o" > "$T/o2"; mv "$T/o2" "$T/o"
sed 's/^/      /' "$T/o"
[ -n "${GATE_DEBUG:-}" ] && cp "$T/e" /var/tmp/sg-displaysvc-debug.log
w() { sed -n "s/^$1 //p" "$T/o" | head -1; }
grep -q "^DONE" "$T/o" || { fail "the probe did not finish"; echo "RESULT: FAIL"; exit 1; }
[ "$(w DOC)" = 1 ] && [ "$(w CREATE)" = 00000000 ] && pass "a shown document makes display pointers" || fail "setup: $(w DOC) $(w CREATE) $(w SETUP)"
[ "$(w HIT)" = "00000000 c 1" ] && [ "$(w HITAFTER)" = e ] && pass "MoveToPoint finds the character under a point, after it past its middle" || fail "hit: $(w HIT) / $(w HITAFTER)"
[ "$(w COMPARE)" = "1 1" ] && pass "IsLeftOf and IsEqualTo" || fail "compare: $(w COMPARE)"
[ "$(w NEXTLINE)" = "00000000 i" ] && pass "MoveUnit NextLine keeps the x on the line below" || fail "next line: $(w NEXTLINE)"
[ "$(w LINESTART)" = "00000000 g" ] && [ "$(w BOL)" = "0 1" ] && pass "CurrentLineStart, and IsAtBOL only there" || fail "line start: $(w LINESTART) bol $(w BOL)"
[ "$(w BREAKS)" = "0 2" ] && pass "QueryBreaks reports the <br> before the line" || fail "breaks: $(w BREAKS)"
[ "$(w LINEINFO)" = "1 1" ] && pass "GetLineInfo: the line's x and text height" || fail "line info: $(w LINEINFO)"
[ "$(w LINEEND)" = "00000000 #" ] && pass "CurrentLineEnd goes past the line's last character" || fail "line end: $(w LINEEND)"
[ "$(w PREVLINE)" = "00000000 a" ] && pass "MoveUnit PreviousLine at a given x" || fail "previous line: $(w PREVLINE)"
[ "$(w FLOW)" = p1 ] && pass "GetFlowElement is the paragraph" || fail "flow: $(w FLOW)"
[ "$(w CARET)" = 00000000 ] && [ "$(w CARETAT)" = "00000000 c" ] && pass "GetCaret; the caret moves to a display pointer and a markup pointer to it" || fail "caret: $(w CARET) $(w CARETAT)"
[ "$(w CARETLOC)" = 1 ] && pass "GetLocation is the caret's box" || fail "caret location: $(w CARETLOC) (layout $(w LAYOUT))"
[ "$(w INSERT)" = "00000000 abXYcdef c" ] && pass "InsertText inserts at the caret and leaves it after" || fail "insert: $(w INSERT)"
[ "$(w HIDE)" = 1 ] && pass "Hide hides the caret" || fail "hide: $(w HIDE)"
case "$(w STYLE)" in "1 1 700 0000ff "?*) pass "GetComputedStyle: bold, 700, underline, red, a face" ;; *) fail "style: $(w STYLE)" ;; esac
[ "$(w PLAIN)" = "1 1 1" ] && pass "plain text is not bold, unlike it, like its paragraph" || fail "plain: $(w PLAIN)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
