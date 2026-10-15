#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# inetcomm: IMimeMessage::InsertBody and DeleteBody edit the body tree.
#
#   WINE=/opt/wine-sg/bin/wine test/fastinetedit-gate.sh   (mutants SG_MUTANT_INET_NODELETE, SG_MUTANT_INET_INSERTORDER)
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
T=$(mktemp -d /var/tmp/sg-fastinetedit.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastinetedit-probe.c" -idirafter "${WINE_INCLUDE:-/opt/wine-sg/include/wine/windows}" -lole32 -loleaut32 -luuid -linetcomm || { fail "probe did not build"; exit 1; }
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
[ "$(w INS_LAST)" = "00000000 1" ] && [ "$(w INS_FIRST)" = 00000000 ] && [ "$(w INS_NEXT)" = 00000000 ] && [ "$(w INS_PREV)" = 00000000 ] && pass "InsertBody adds a body last, first, after and before a pivot" || fail "insert: $(w INS_LAST) $(w INS_FIRST) $(w INS_NEXT) $(w INS_PREV)"
[ "$(w COUNT1)" = "00000000 5" ] && pass "CountBodies counts the inserted bodies" || fail "count: $(w COUNT1)"
[ "$(w ORDER1)" = "$(w HANDLES)" ] && pass "the children come in the order previous, first, next, last" || fail "order: $(w ORDER1) / $(w HANDLES)"
[ "$(w INS_ROOT_TWICE)" = 800cce09 ] && [ "$(w INS_PARENT)" = 80070057 ] && [ "$(w INS_NEXT_ROOT)" = 800cce09 ] && pass "a second root, a parent location and a sibling of the root are refused" || fail "refused: $(w INS_ROOT_TWICE) $(w INS_PARENT) $(w INS_NEXT_ROOT)"
[ "$(w DEL)" = 00000000 ] && [ "$(w ORDER2)" = "$(w HANDLES2)" ] && [ "$(w COUNT2)" = "00000000 4" ] && [ "$(w DEL_AGAIN)" = 800cce09 ] && pass "DeleteBody removes a body and the handle is then invalid" || fail "delete: $(w DEL) $(w COUNT2) $(w DEL_AGAIN)"
[ "$(w DEL_FLAGS)" = 800cce2f ] && pass "unknown delete flags give MIME_E_INVALID_DELETE_TYPE" || fail "flags: $(w DEL_FLAGS)"
[ "$(w INS_UNDER)" = 00000000 ] && [ "$(w DEL_WITH_KIDS)" = 00000000 ] && [ "$(w COUNT3)" = "00000000 3" ] && pass "deleting a body takes its children along" || fail "kids: $(w INS_UNDER) $(w DEL_WITH_KIDS) $(w COUNT3)"
[ "$(w DEL_ROOT)" = 00000000 ] && [ "$(w NOROOT)" = 800cce05 ] && [ "$(w NEWROOT)" = "00000000 1" ] && [ "$(w COUNT4)" = "00000000 1" ] && pass "after deleting the root a new root can be inserted" || fail "root: $(w DEL_ROOT) $(w NOROOT) $(w NEWROOT) $(w COUNT4)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
