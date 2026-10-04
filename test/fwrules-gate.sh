#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Firewall rules a program adds are kept (patches/sg/0792): installers that
# set up Windows Firewall -- Sonos's (David 2026-10-02: it failed there) --
# add a rule through INetFwPolicy2::Rules, then look for it, list the rules,
# change or remove it. Wine's collection was a stub (none ever, Item and
# For Each failing). Here: a program adds a rule; the count goes up; it is
# listed (_NewEnum) and found (Item) with what was set; a change to it is
# kept; Remove takes it away. A script does the same with For Each. The rule
# is where Windows keeps it, in its form.
#
#   WINE=/opt/wine-sg/bin/wine test/fwrules-gate.sh   (mutant SG_MUTANT_FW_RULES_STUB)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fwrules.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/fwrules-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/o"
v() { sed -n "s/^$1 //p" "$T/o" | head -1; }
grep -q '^DONE' "$T/o" || { fail "the probe did not finish: $(tail -2 "$T/o" | tr '\n' ' ')"; }
[ "$(v ADD)" = 00000000 ] && [ "$(v COUNT1)" = $(( $(v COUNT0) + 1 )) ] \
    && pass "a rule added is kept: the count goes from $(v COUNT0) to $(v COUNT1)" || fail "add: $(v ADD), count $(v COUNT0) -> $(v COUNT1)"
[ "$(v LISTED0)" = 0 ] && [ "$(v LISTED1)" = 1 ] && pass "For Each lists it once (_NewEnum)" || fail "listed before/after: $(v LISTED0)/$(v LISTED1)"
[ "$(v APP)" = 'C:\Program Files\Gate\server.exe' ] && [ "$(v PORTS)" = 1400,3400 ] && [ "$(v PROTO)" = 6 ] && [ "$(v PROFILES)" = 3 ] && [ "$(v ENABLED)" = 1 ] \
    && pass "Item finds it with what was set (program, ports 1400,3400, TCP, Domain+Private, on)" \
    || fail "Item: app '$(v APP)' ports '$(v PORTS)' proto '$(v PROTO)' profiles '$(v PROFILES)' enabled '$(v ENABLED)' $(v ITEM)"
[ "$(v ENABLED2)" = 0 ] && pass "turning the rule got off is kept (a live rule)" || fail "after Enabled = False: '$(v ENABLED2)'"
[ "$(v FWON)" = "00000000 1" ] && [ "$(v PROFILE)" = "00000000 2" ] && pass "the policy answers: firewall on, the Private profile" || fail "policy: FirewallEnabled '$(v FWON)' CurrentProfileTypes '$(v PROFILE)'"
[ "$(v GROUPON)" = 00000000 ] && [ "$(v GROUPENABLED)" = "00000000 1" ] && pass "its rule group turned on is on" || fail "group: enable '$(v GROUPON)' enabled '$(v GROUPENABLED)'"
case "$(v REMOVE)" in "00000000 COUNT2 $(v COUNT0)") pass "Remove takes it away";; *) fail "remove: $(v REMOVE)";; esac
[ "$(v ITEMGONE)" = 80070002 ] && pass "a rule not there is not found (0x80070002)" || fail "Item after Remove: $(v ITEMGONE)"
# where Windows keeps it, while there (the script's rule, before its Remove: read the stored form)
cp "$HERE/fwrules.vbs" "$WINEPREFIX/drive_c/fwrules.vbs"
out=$(timeout 120 "$WINE" cscript //nologo 'C:\fwrules.vbs' 2>/dev/null | tr -d '\r')
[ "$out" = "VBS 1 1900" ] && pass "a script: Add, then For Each finds it (VBS 1 1900)" || fail "script: '$out'"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
