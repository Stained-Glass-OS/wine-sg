#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Firewall rules programs add reach Stained Glass Firewall (patches/sg/1501,
# 1502). Installers add rules with netsh advfirewall (and the older netsh
# firewall), INetFwPolicy2 or the older INetFwMgr; the rules are kept where
# Windows keeps them (0792), and each is now also handed to the firewall
# through sg-admind at once (its spool: SG_ADMIN_SPOOL here), and the
# firewall's state is the firewall's (its status: SG_FIREWALL_STATUS here):
#   - netsh advfirewall firewall add/show/delete rule, in Windows' words
#   - netsh advfirewall set/show ...profile state, the current profile Public
#   - netsh firewall add allowedprogram / portopening; INetFwMgr's
#     AuthorizedApplications.Add and GloballyOpenPorts.Add (a script)
#   - without an administrator's spool: "requires elevation"
#
#   WINE=/opt/wine-sg/bin/wine test/fwnetsh-gate.sh
#   (mutants SG_MUTANT_FW_RULES_NOT_PUSHED in dlls/hnetcfg/policy.c,
#    SG_MUTANT_NETSH_FIREWALL_STUB in programs/netsh/netsh.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fwnetsh.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export SG_ADMIN_SPOOL="$T/spool" SG_FIREWALL_STATUS="$T/status"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || { chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"; }' EXIT INT TERM
mkdir -p "$T/spool/requests" "$T/spool/replies"
printf 'state\trunning\nprofile\tdomain\ton\nprofile\tprivate\ton\nprofile\tpublic\toff\ncurrent\tpublic\n' > "$T/status"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1

netsh() { timeout 120 "$WINE" netsh "$@" 2>/dev/null | tr -d '\r'; }
# the requests filed, as one line each: fields joined by " / "
requests() { for f in "$T"/spool/requests/*.req; do [ -f "$f" ] && { tr '\n' '|' < "$f" | sed 's/|$//; s/|/ \/ /g'; echo; }; done; }
clear_requests() { rm -f "$T"/spool/requests/*.req; }

out=$(netsh advfirewall firewall add rule name="Gate Server" dir=in action=allow 'program=C:\Program Files\Gate\server.exe' protocol=TCP localport=3400 profile=private enable=yes)
printf '%s\n' "$out" | grep -qx 'Ok.' && pass "netsh advfirewall firewall add rule: Ok." || fail "add rule: $out"
r=$(requests)
want='firewall / registry-push / Gate Server / v2.30 / Action=Allow / Active=TRUE / Dir=In / Protocol=6 / Profile=Private / LPort=3400 / App=C:\Program Files\Gate\server.exe / Name=Gate Server / '
printf '%s\n' "$r" | grep -qxF "$want" && pass "the rule is handed to the firewall at once (sg-admind: firewall registry-push), in Windows' form" \
    || fail "requests: $r"
out=$(netsh advfirewall firewall show rule name="Gate Server")
printf '%s\n' "$out" | grep -q '^LocalPort: *3400$' && printf '%s\n' "$out" | grep -q '^Profiles: *Private$' \
    && printf '%s\n' "$out" | grep -q '^Action: *Allow$' && pass "show rule: it is listed as added" || fail "show rule: $out"
out=$(netsh advfirewall show currentprofile)
printf '%s\n' "$out" | grep -q '^Public Profile Settings' && printf '%s\n' "$out" | grep -q '^State *OFF$' \
    && pass "show currentprofile: the firewall's own (Public, off)" || fail "show currentprofile: $out"
clear_requests
out=$(netsh advfirewall set publicprofile state on)
printf '%s\n' "$out" | grep -qx 'Ok.' && [ "$(requests)" = "firewall / profile / public / on" ] \
    && pass "set publicprofile state on: asked of the firewall" || fail "set state: $out / $(requests)"
clear_requests
out=$(netsh advfirewall set allprofiles state off)
[ "$(requests)" = "firewall / profile / all / off" ] && pass "set allprofiles state off" || fail "allprofiles: $out / $(requests)"
clear_requests
out=$(netsh advfirewall firewall delete rule name="Gate Server")
printf '%s\n' "$out" | grep -q '^Deleted 1 rule(s).' && [ "$(requests)" = "firewall / registry-push / Gate Server" ] \
    && pass "delete rule: Deleted 1 rule(s), and the firewall told" || fail "delete: $out / $(requests)"
out=$(netsh advfirewall firewall delete rule name="Gate Server")
printf '%s\n' "$out" | grep -q 'No rules match the specified criteria.' && pass "deleting it again: no rules match" || fail "delete again: $out"
clear_requests
out=$(netsh firewall add allowedprogram 'C:\Legacy\legacy.exe' Legacy ENABLE)
printf '%s\n' "$out" | grep -qx 'Ok.' && requests | grep -qF 'App=C:\Legacy\legacy.exe / Name=Legacy /' \
    && pass "netsh firewall add allowedprogram (the older context)" || fail "allowedprogram: $out / $(requests)"
clear_requests
out=$(netsh firewall add portopening TCP 8123 "Legacy port")
printf '%s\n' "$out" | grep -qx 'Ok.' && requests | grep -qF 'Protocol=6 / LPort=8123 / Name=Legacy port /' \
    && pass "netsh firewall add portopening" || fail "portopening: $out / $(requests)"
clear_requests
cp "$HERE/fwnetsh-v1.vbs" "$WINEPREFIX/drive_c/fwnetsh-v1.vbs"
out=$(timeout 120 "$WINE" cscript //nologo 'C:\fwnetsh-v1.vbs' 2>/dev/null | tr -d '\r')
r=$(requests)
[ "$out" = "V1 OK 4 0 -1" ] && pass "a script with INetFwMgr: works; the policy says Public, off on Public, on on Private" || fail "script: '$out'"
printf '%s\n' "$r" | grep -qF 'App=C:\Old\app.exe / Name=Old App /' && printf '%s\n' "$r" | grep -qF 'Protocol=17 / LPort=5555 / Name=Old Port /' \
    && pass "INetFwMgr's authorized application and open port become rules, handed to the firewall" || fail "v1: $r"
clear_requests
chmod 500 "$T/spool/requests"
out=$(netsh advfirewall set allprofiles state off)
printf '%s\n' "$out" | grep -q 'requires elevation' && pass "without an administrator's spool: requires elevation" || fail "not elevated: $out"
chmod 700 "$T/spool/requests"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
