#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: the legacy event object (writable properties, named attributes, defaults of the rest) and DOM mouse/keyboard events.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlevent-gate.sh   (mutant SG_MUTANT_MSHTML_EVENT_NOSET)
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
T=$(mktemp -d /var/tmp/sg-fastmshtmlevent.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmlevent-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
[ "$(v MODE)" = 11 ] || fail "document mode: $(v MODE)"
[ "$(w EVOBJ)" = "00000000 1" ] && [ "$(w IFACES)" = "1 1 1 1 1" ] && pass "createEventObject gives an event object with all its interfaces" || fail "event object: $(w EVOBJ) $(w IFACES)"
[ "$(w KEYCODE)" = "00000000 65" ] && pass "put_keyCode is kept" || fail "keyCode: $(w KEYCODE)"
[ "$(w BUTTON)" = "00000000 00000000 11 11" ] && [ "$(w REASON)" = "00000000 00000000 12 12" ] && [ "$(w X)" = "00000000 00000000 13 13" ] && [ "$(w Y)" = "00000000 00000000 14 14" ] \
    && [ "$(w CLIENTX)" = "00000000 00000000 15 15" ] && [ "$(w CLIENTY)" = "00000000 00000000 16 16" ] && [ "$(w OFFSETX)" = "00000000 00000000 17 17" ] \
    && [ "$(w OFFSETY)" = "00000000 00000000 18 18" ] && [ "$(w SCREENX)" = "00000000 00000000 19 19" ] && [ "$(w SCREENY)" = "00000000 00000000 20 20" ] \
    && pass "button, reason and the coordinates are writable and read back through both interfaces" || fail "long properties: $(grep -E '^(BUTTON|REASON|X|Y|CLIENT|OFFSET|SCREEN)' "$T/o" | tr '\n' ';')"
[ "$(w MODS)" = "00000000 00000000 -1 -1 -1 0" ] && [ "$(w REPEAT)" = "00000000 -1" ] && [ "$(w LEFT)" = "-1 0 -1" ] && pass "alt/ctrl/shift, repeat and the left-hand modifiers are writable" || fail "booleans: $(w MODS) / $(w REPEAT) / $(w LEFT)"
[ "$(w TYPE)" = "00000000 00000000 mytype" ] && [ "$(w QUALIFIER)" = "00000000 00000000 qual" ] && [ "$(w SRCURN)" = "00000000 00000000 urn:x" ] && [ "$(w DATAFLD)" = "00000000 00000000 fld" ] \
    && [ "$(w PROPNAME)" = "00000000 00000000 prop" ] && [ "$(w DATA)" = "00000000 00000000 payload" ] && [ "$(w ISSESSION)" = "00000000 00000000 -1" ] && pass "type, qualifier, srcUrn, dataFld, propertyName, data and issession are writable" || fail "strings: $(grep -E '^(TYPE|QUAL|SRCURN|DATA|PROPNAME|ISSES)' "$T/o" | tr '\n' ';')"
[ "$(w SRCELEM)" = "00000000 00000000 d1" ] && [ "$(w TOELEM)" = "00000000 00000000 d2" ] && [ "$(w FROMELEM)" = "00000000 00000000 d1" ] && [ "$(w SRCNULL)" = "00000000 00000000 1" ] \
    && [ "$(w SRCFILTER)" = "00000000 00000000 1" ] && [ "$(w RECORDSET)" = "00000000 00000000 1" ] && pass "srcElement, toElement, fromElement, srcFilter and recordset keep what they are given, NULL included" || fail "objects: $(grep -E '^(SRC|TOELEM|FROM|RECORD)' "$T/o" | tr '\n' ';')"
[ "$(w ATTR0)" = "00000000 00000000 3 77" ] && [ "$(w ATTR1)" = "00000000 1" ] && [ "$(w ATTR2)" = "00000000 0 00000000 -1 00000000 0" ] && [ "$(w ATTR3)" = "00000000 1" ] \
    && pass "setAttribute/getAttribute/removeAttribute: case-insensitive by default, flag 1 is case-sensitive" || fail "attributes: $(grep ^ATTR "$T/o" | tr '\n' ';')"
[ "$(w OVERFLOW)" = "00000000 0" ] && [ "$(w IME)" = "00000000 0 00000000 0 00000000 0" ] && [ "$(w BEHAVIOR)" = "00000000 00000000 0 0" ] && [ "$(w WHEEL)" = "00000000 0" ] \
    && [ "$(w EV6)" = "00000000 00000000 1 0" ] && [ "$(w DATATRANSFER)" = "00000000 1" ] && pass "contentOverflow, IME, behavior, wheelDelta, actionURL, buttonID, dataTransfer answer instead of E_NOTIMPL" || fail "defaults: $(grep -E '^(OVERFLOW|IME|BEHAV|WHEEL|EV6|DATATR)' "$T/o" | tr '\n' ';')"
[ "$(v MEV0)" = "true false true" ] && pass "MouseEvent.getModifierState follows initMouseEvent" || fail "getModifierState: $(v MEV0)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
