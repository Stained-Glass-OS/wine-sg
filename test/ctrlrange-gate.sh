#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: body.createControlRange() and IHTMLControlRange (6047) -- it gave E_NOTIMPL. Editors in older
# programs select an image or a table as a control range and run commands on it.
#   WINE=/opt/wine-sg/bin/wine test/ctrlrange-gate.sh   (mutants SG_MUTANT_CTRLRANGE_STUB, SG_MUTANT_CTRLRANGE_FIRST_PARENT, SG_MUTANT_CTRLELEM_NONE)
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
T=$(mktemp -d /var/tmp/sg-ctrlrange.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/ctrlrange-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
[ "$(w CREATE)" = 00000000 ] && [ "$(w IFACE)" = 1 ] && [ "$(w LEN0)" = 0 ] && pass "createControlRange gives an empty IHTMLControlRange" || fail "create: $(w CREATE) $(w IFACE) $(w LEN0)"
[ "$(w ADD)" = 00000000 ] && [ "$(w LEN1)" = 1 ] && [ "$(w LEN2)" = 2 ] && pass "add: an element is in the range once" || fail "add: $(w ADD) $(w LEN1) $(w LEN2)"
[ "$(w ITEM1)" = t1 ] && [ "$(w ITEMBAD)" = 80070057 ] && pass "item: the elements in order, E_INVALIDARG past the end" || fail "item: $(w ITEM1) $(w ITEMBAD)"
[ "$(w PARENT)" = box ] && [ "$(w PARENT2)" = BODY ] && pass "commonParentElement: the nearest element that holds them all" || fail "parent: $(w PARENT) $(w PARENT2)"
[ "$(w REMOVE)" = 00000000 ] && [ "$(w LEN3)" = 1 ] && [ "$(w ITEM0)" = t1 ] && pass "remove takes one out" || fail "remove: $(w REMOVE) $(w LEN3) $(w ITEM0)"
[ "$(w SELECT)" = 00000000 ] && [ "$(w SUPP)" = "00000000 -1" ] && pass "select, queryCommandSupported" || fail "select: $(w SELECT) $(w SUPP)"
[ "$(w EXEC)" = "00000000 -1" ] && [ "$(w TABLE_AFTER)" = 0 ] && [ "$(w IMG_AFTER)" = 1 ] && pass "execCommand(Delete) deletes the range's element, only it" || fail "delete: $(w EXEC) table=$(w TABLE_AFTER) img=$(w IMG_AFTER)"
[ "$(w SCRIPT)" = "00000000 len=1 id=i1" ] && pass "from script: createControlRange, add, length, item" || fail "script: $(w SCRIPT)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
