#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: execCommand and queryCommand* on a text range run on the range, with the document selection left as it was.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlrangecmd-gate.sh   (mutants SG_MUTANT_MSHTML_RANGECMD_NORESTORE, SG_MUTANT_MSHTML_RANGECMD_NOAPPLY)
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
T=$(mktemp -d /var/tmp/sg-fastmshtmlrangecmd.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmlrangecmd-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
[ "$(w DESIGN)" = 00000000 ] && [ "$(w CONTENT)" = 00000000 ] && pass "design mode on, content set" || fail "setup: $(w DESIGN) $(w CONTENT)"
[ "$(w SUPP)" = "00000000 -1" ] && [ "$(w ENAB)" = "00000000 -1" ] && [ "$(w SUPPNO)" = "00000000 0" ] && pass "queryCommandSupported/Enabled on a range" || fail "support: $(w SUPP) $(w ENAB) $(w SUPPNO)"
[ "$(w STATE0)" = "0 0" ] && [ "$(w EXEC)" = "00000000 -1" ] && [ "$(w D1A)" = 'Hello <i>big</i> <b>World</b>' ] && pass "execCommand(bold) formats just the range" || fail "bold: $(w STATE0) $(w EXEC) $(w D1A)"
[ "$(w STATE1)" = "-1 0" ] && [ "$(w WORLD2)" = "[World]" ] && pass "queryCommandState follows the formatting of each range; the range still spans its text" || fail "state: $(w STATE1) $(w WORLD2)"
[ "$(w SEL)" = "[second]" ] && pass "the selection of the document is as it was before the commands" || fail "selection: $(w SEL)"
[ "$(w EXECNO)" = 80040100 ] && pass "an unknown command is refused like the document does" || fail "unknown: $(w EXECNO)"
[ "$(w D1B)" = '<i>Hello</i> <i>big</i> <b>World</b>' ] && [ "$(w D1C)" = '<u><i>Hello</i></u> <i>big</i> <b>World</b>' ] && [ "$(w CTEXEC)" = 00000000 ] && pass "a second range, and the IOleCommandTarget of a range, edit their own text" || fail "second: $(w D1B) $(w D1C) $(w CTEXEC)"
[ "$(w FONT)" = "00000000 -1" ] && [ "$(w FONTVAL)" = "00000000 8 Arial" ] && [ "$(w BOLDVAL)" = "00000000 11 -1" ] && pass "queryCommandValue: font name as a string, a toggle as its state" || fail "values: $(w FONT) $(w FONTVAL) $(w BOLDVAL)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
