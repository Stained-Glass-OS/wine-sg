#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: markup pointers (positions, Left/Right over tokens, comparisons, markup positions) and the editing of IMarkupServices.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlmarkup-gate.sh   (mutants SG_MUTANT_MSHTML_MARKUP_NONORM, SG_MUTANT_MSHTML_MARKUP_NOVOID)
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
T=$(mktemp -d /var/tmp/sg-fastmshtmlmarkup.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmlmarkup-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
[ "$(w MS)" = 00000000 ] && pass "the document has IMarkupServices" || fail "MS: $(w MS)"
[ "$(w POS0)" = "0 00000000 1 00000000 1 00000000 1" ] && [ "$(w GRAV)" = "0 1" ] && [ "$(w CLING)" = 1 ] && [ "$(w UNPOS)" = 0 ] && pass "a pointer is unpositioned at first; positioned it has a container and an owning document; gravity, cling and Unposition" || fail "state: $(w POS0) $(w GRAV) $(w CLING) $(w UNPOS)"
[ "$(w RIGHT)" = "T:Hello |E:B|T:World|X:B|T:!|X:DIV|E:P|T:x|X:P|N:BR" ] && pass "Right moves over text, begin tags, end tags and no-scope elements" || fail "right: $(w RIGHT)"
[ "$(w LEFT)" = "X:DIV|T:!|X:B|T:World|E:B|T:Hello |E:DIV|E:BODY|X:HEAD|N:META" ] && [ "$(w NOSCOPE)" = "N:BR" ] && pass "Left moves the other way over the same tokens" || fail "left: $(w LEFT) $(w NOSCOPE)"
[ "$(w CMP)" = "1 0 1 1 1 1" ] && [ "$(w TEXTEND)" = "1 6 1" ] && pass "IsLeftOf/IsRightOf/IsEqualTo; the end of a text equals the position after its node" || fail "compare: $(w CMP) $(w TEXTEND)"
[ "$(w MP)" = "00000000 15" ] && [ "$(w MOVEMP)" = "00000000 1" ] && [ "$(w MOVEMP2)" = "00000000 1" ] && pass "GetMarkupPosition counts tokens and characters; MoveToMarkupPosition goes back to them" || fail "markup position: $(w MP) $(w MOVEMP) $(w MOVEMP2)"
[ "$(w TAG1)" = 1 ] && [ "$(w TAG2)" = "00000000 table" ] && [ "$(w TAG3)" = 1 ] && [ "$(w SCOPED)" = "0 1" ] && pass "tag ids and names; scoped elements" || fail "tags: $(w TAG1) $(w TAG2) $(w TAG3) $(w SCOPED)"
[ "$(w CREATE)" = "00000000 SPAN s1 k m" ] && [ "$(w CLONE)" = "00000000 [(null)] s1" ] && [ "$(w CREATEBAD)" = 80070057 ] && pass "CreateElement with attributes; CloneElement copies the attributes without the content" || fail "create: $(w CREATE) $(w CLONE) $(w CREATEBAD)"
[ "$(w D1A)" = 'Hello <b id="b1">World</b>!??' ] && [ "$(w D1B)" = 'Hello <span id="s1"><b id="b1">World</b></span>!??' ] && [ "$(w D1C)" = 'Hello <b id="b1">World</b>!??' ] && pass "InsertText, InsertElement around content, RemoveElement keeping the content" || fail "edit: $(w D1A) / $(w D1B) / $(w D1C)"
[ "$(w P1A)" = "Hello x" ] && [ "$(w D1D)" = 'Hello <b id="b1">World</b>!??' ] && [ "$(w P1B)" = "Hello Hello x" ] && [ "$(w D1E)" = '<b id="b1">World</b>!??' ] && [ "$(w COPYSELF)" = "00000000" ] && pass "Copy leaves the source, Move takes it" || fail "copy/move: $(w P1A) / $(w D1D) / $(w P1B) / $(w D1E)"
[ "$(w REMOVEBAD)" = 80070057 ] && [ "$(w D1F)" = '<b id="b1"></b>!??' ] && pass "Remove deletes between the pointers and refuses them in the wrong order" || fail "remove: $(w REMOVEBAD) $(w D1F)"
[ "$(w PTR2RANGE)" = "00000000 1 1" ] && [ "$(w RANGE2PTR)" = "00000000 [ab cd]" ] && pass "MovePointersToRange and MoveRangeToPointers" || fail "ranges: $(w PTR2RANGE) $(w RANGE2PTR)"
[ "$(w FIND)" = "00000000 cd ab cd" ] && [ "$(w FINDNO)" = 00000001 ] && [ "$(w FINDCASE)" = 00000001 ] && [ "$(w UNIT)" = "00000000 b " ] && pass "FindText (match end pointer, no match, case) and MoveUnit by a character" || fail "find: $(w FIND) $(w FINDNO) $(w FINDCASE) $(w UNIT)"
[ "$(w PARSE)" = "00000000 00000000 1 2 U 1" ] && [ "$(w PARSEGLOBAL)" = "00000000 1" ] && pass "ParseString positions the pointers over what it parsed; OwningDoc; ParseGlobal" || fail "parse: $(w PARSE) $(w PARSEGLOBAL)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
