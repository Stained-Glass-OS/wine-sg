#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: text range units (sentence, textedit) for move/moveStart/moveEnd/expand, and bookmarks.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlunits-gate.sh   (mutants SG_MUTANT_RANGE_NOSENTENCE, SG_MUTANT_RANGE_NOBOOKMARK)
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
T=$(mktemp -d /var/tmp/sg-fastmshtmlunits.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmlunits-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
[ "$(v MODE)" = 11 ] || fail "document mode: $(v MODE)"
[ "$(v MS0)" = 1 ] && [ "$(v MS1)" = "[Three four! ]" ] && [ "$(v MS2)" = "1[Five six?]" ] && pass "move(sentence) steps over a sentence with its white space" || fail "move sentence: $(grep '^MS[0-9]=' "$T/o" | tr '\n' ' ')"
[ "$(v MS3)" = "-1[Three four! ]" ] && [ "$(v MS4)" = -1 ] && pass "move(sentence, negative) goes back to the start of the previous sentence and stops at the start" || fail "move back: $(v MS3) $(v MS4)"
[ "$(v MSS0)" = "[One two. Three four! ]" ] && [ "$(v MSS1)" = "1[Three four! ]" ] && [ "$(v MSS2)" = "-1[]" ] && pass "moveStart/moveEnd by sentence" || fail "moveStart/End: $(v MSS0) $(v MSS1) $(v MSS2)"
[ "$(v EXP0)" = "true[Three four! ]" ] && [ "$(v EXP1)" = "true[Three four! ]" ] && pass "expand(sentence) covers the sentence around the point" || fail "expand: $(v EXP0) $(v EXP1)"
[ "$(v TE0)" = "1[ two. Three four! Five six?Next para.]" ] && [ "$(v TE1)" = "-1[One two. Three four! Five six?Next para.]" ] && [ "$(v TE2)" = 0 ] && pass "moveEnd/moveStart by textedit go to the end/start of the body" || fail "textedit: $(v TE0) $(v TE1) $(v TE2)"
[ "$(v TE3)" = "1[]" ] && [ "$(v TE4)" = true ] && pass "move(textedit) collapses at the end of the text" || fail "move textedit: $(v TE3) $(v TE4)"
[ "$(v BM0 | cut -d' ' -f1)" = string ] && [ "$(v BM1)" = "true true" ] && [ "$(v BM2)" = "true true" ] && pass "getBookmark/moveToBookmark restore a range, also onto another range" || fail "bookmark: $(v BM0) $(v BM1) $(v BM2)"
[ "$(v BM3)" = "false false" ] && pass "an invalid bookmark is refused with false" || fail "bad bookmark: $(v BM3)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
