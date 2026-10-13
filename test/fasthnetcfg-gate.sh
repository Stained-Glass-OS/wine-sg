#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# hnetcfg: the firewall's profile settings, the older interfaces and
# INetFwMgr's questions, backed by Stained Glass Firewall's status file and
# sg-admind's spool (stand-ins here; the host firewall is never touched).
#
#   WINE=/opt/wine-sg/bin/wine test/fasthnetcfg-gate.sh   (mutant SG_MUTANT_HN_NO_SCOPE)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fasthn.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
export SG_ADMIN_SPOOL="$T/spool" SG_FIREWALL_STATUS="$T/status"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || { chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"; }' EXIT INT TERM
mkdir -p "$T/spool/requests" "$T/spool/replies"
printf 'state\trunning\nprofile\tdomain\ton\nprofile\tprivate\ton\nprofile\tpublic\toff\ncurrent\tprivate\n' > "$T/status"
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/fasthnetcfg-probe.c" -lole32 -loleaut32 -luuid -ladvapi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/o"
v() { sed -n "s/^$1 //p" "$T/o" | head -1; }
grep -q '^DONE' "$T/o" || fail "the probe did not finish: $(tail -2 "$T/o" | tr '\n' ' ')"
[ "$(v D_BLOCKALL)" = 0 ] && [ "$(v D_NOTIF)" = 0 ] && [ "$(v D_UNI)" = 0 ] && [ "$(v D_IN)" = 0 ] && [ "$(v D_OUT)" = 1 ] \
    && pass "defaults: not blocked outright, notifications on, inbound blocked, outbound allowed" || fail "defaults: $(grep '^D_' "$T/o" | tr '\n' ' ')"
[ "$(v D_ALLGET)" = 80070057 ] && pass "a read names one profile (ALL: E_INVALIDARG)" || fail "get ALL: $(v D_ALLGET)"
[ "$(v D_ICMP8)" = "0 ICMP3 1" ] && pass "ICMP defaults: echo request refused, destination unreachable allowed" || fail "icmp defaults: $(v D_ICMP8)"
[ "$(v S_BLOCK)" = 00000000 ] && [ "$(v S_BLOCK_PUB)" = 1 ] && [ "$(v S_BLOCK_PRIV)" = 0 ] && pass "block all inbound is kept per profile" || fail "block: $(grep '^S_BLOCK' "$T/o" | tr '\n' ' ')"
[ "$(v S_OUT)" = 00000000 ] && [ "$(v S_OUT_DOM)" = 0 ] && [ "$(v S_OUT_PRIV)" = 0 ] && pass "ALL sets every profile (default outbound action: block)" || fail "outbound: $(grep '^S_OUT' "$T/o" | tr '\n' ' ')"
[ "$(v S_BADACT)" = 80070057 ] && [ "$(v S_NOTIF)" = 1 ] && pass "an invalid action is refused; notifications disabled is kept" || fail "badact/notif: $(v S_BADACT) $(v S_NOTIF)"
[ "$(v X_PUT)" = 00000000 ] && [ "$(v X_ITEMS)" = "2 Ethernet Wi-Fi" ] && [ "$(v X_PUB)" = "00000000 0" ] && pass "excluded interfaces round-trip per profile" || fail "excluded: $(grep '^X_' "$T/o" | tr '\n' ' ')"
[ "$(v L_CURTYPE)" = 1 ] && [ "$(v L_GETDOM)" = 00000000 ] && [ "$(v L_BADTYPE)" = 80070057 ] && pass "INetFwMgr: current profile Standard; GetProfileByType" || fail "legacy types: $(grep '^L_CUR\|^L_GET\|^L_BAD' "$T/o" | tr '\n' ' ')"
[ "$(v L_DOMTYPE)" = 0 ] && [ "$(v L_STDTYPE)" = 1 ] && [ "$(v L_CURPTYPE)" = 1 ] && pass "INetFwProfile::Type" || fail "profile types: $(grep '^L_.*TYPE' "$T/o" | tr '\n' ' ')"
[ "$(v L_EXC_DOM)" = 1 ] && [ "$(v L_EXC_STD)" = 0 ] && [ "$(v L_EXC_P2DOM)" = 1 ] && [ "$(v L_NOTIF_PUB)" = 1 ] \
    && pass "the older profile and INetFwPolicy2 share the settings (Domain; Standard = Private and Public)" || fail "shared: $(grep '^L_EXC\|^L_NOTIF' "$T/o" | tr '\n' ' ')"
[ "$(v L_FWON_STD)" = 1 ] && [ "$(v L_FWON_DOM)" = 1 ] && pass "FirewallEnabled follows the firewall's status" || fail "fwon: $(grep '^L_FWON' "$T/o" | tr '\n' ' ')"
[ "$(v PA_NONE)" = 0 ] && pass "IsPortAllowed: no rule, not allowed" || fail "pa none: $(v PA_NONE)"
[ "$(v PA_3400)" = "1 PA_4005 1 PA_3401 0 PA_UDP 0" ] && pass "IsPortAllowed: the rule's ports (3400, 4000-4010), TCP only" || fail "pa ports: $(v PA_3400)"
[ "$(v PA_RESTR)" = 0 ] && [ "$(v PA_SCOPED)" = 1 ] && pass "restricted when the rule has a remote scope" || fail "restricted: $(v PA_RESTR) / $(v PA_SCOPED)"
[ "$(v PA_BLOCKALL)" = 0 ] && pass "block all inbound beats the rule" || fail "blockall: $(v PA_BLOCKALL)"
[ "$(v PA_APP)" = "1 PA_OTHERAPP 0" ] && pass "a program's rule is for that program (case-insensitive path)" || fail "pa app: $(v PA_APP)"
[ "$(v PA_BADPORT)" = 80070057 ] && pass "a port above 65535 is refused" || fail "badport: $(v PA_BADPORT)"
[ "$(v ICMP8)" = "1 ICMP13 0 ICMP99 0" ] && pass "IsIcmpTypeAllowed follows the ICMP settings" || fail "icmp: $(v ICMP8)"
[ "$(v RESTORE)" = 00000000 ] && [ "$(v PRE_COUNT)" = 1 ] && [ "$(v POST_COUNT)" = 0 ] && [ "$(v POST_BLOCK)" = 0 ] && [ "$(v POST_OUT)" = 1 ] && [ "$(v POST_X)" = 0 ] \
    && pass "RestoreLocalFirewallDefaults: settings and rules back to the defaults" || fail "restore: $(grep '^RESTORE\|^PRE_\|^POST_' "$T/o" | tr '\n' ' ')"
reqs=$(for f in "$T"/spool/requests/*.req; do [ -f "$f" ] && { tr '\n' ' ' < "$f"; echo; }; done)
printf '%s\n' "$reqs" | grep -qx 'firewall reset ' && printf '%s\n' "$reqs" | grep -q '^firewall profile public off $' && printf '%s\n' "$reqs" | grep -q '^firewall profile private off $' \
    && pass "the firewall is asked through sg-admind: reset, and profile off for private and public" || fail "requests: $reqs"
[ "$(v MGR_RESTORE)" = 00000000 ] && pass "INetFwMgr::RestoreDefaults" || fail "mgr restore: $(v MGR_RESTORE)"
[ "$(v LA_COUNT0)" = 0 ] && [ "$(v LA_SCOPE0)" = 0 ] && [ "$(v LA_REMOTE)" = LocalSubnet ] && [ "$(v LA_ADD)" = 00000000 ] && [ "$(v LA_COUNT1)" = 1 ] \
    && pass "authorized applications: Add, Count, scope and remote addresses" || fail "apps add: $(grep '^LA_' "$T/o" | head -6 | tr '\n' ' ')"
[ "$(v LA_ITEM)" = 00000000 ] && [ "$(v LA_ITEMDATA)" = "Legacy app|LocalSubnet|1" ] && [ "$(v LA_NOITEM)" = 80070002 ] \
    && pass "Item finds the program (any case) with its name and scope; a missing one is not found" || fail "apps item: $(v LA_ITEM) $(v LA_ITEMDATA) $(v LA_NOITEM)"
[ "$(v LA_ENUM)" = "00000000 1 9" ] && [ "$(v LA_ENUM2)" = "00000001 0" ] && pass "authorized applications: For Each lists it once" || fail "apps enum: $(v LA_ENUM) / $(v LA_ENUM2)"
[ "$(v LA_REMOVE)" = 00000000 ] && [ "$(v LA_COUNT2)" = 0 ] && pass "authorized applications: Remove takes it away" || fail "apps remove: $(v LA_REMOVE) $(v LA_COUNT2)"
[ "$(v LP_COUNT0)" = 0 ] && [ "$(v LP_SCOPE)" = 2 ] && [ "$(v LP_BUILTIN)" = 0 ] && [ "$(v LP_ADD)" = 00000000 ] && [ "$(v LP_COUNT1)" = 1 ] \
    && pass "open ports: Add, Count, a custom scope, not built in" || fail "ports add: $(grep '^LP_' "$T/o" | head -6 | tr '\n' ' ')"
[ "$(v LP_ITEM)" = 00000000 ] && [ "$(v LP_ITEMDATA)" = "Legacy port|192.168.1.0/24" ] && [ "$(v LP_NOITEM)" = 80070002 ] \
    && pass "open ports: Item by port and protocol (UDP, not TCP)" || fail "ports item: $(v LP_ITEM) $(v LP_ITEMDATA) $(v LP_NOITEM)"
[ "$(v LP_ENUM)" = "00000000 1 9" ] && [ "$(v LP_REMOVE)" = 00000000 ] && [ "$(v LP_COUNT2)" = 0 ] && pass "open ports: For Each, Remove" || fail "ports enum/remove: $(v LP_ENUM) $(v LP_REMOVE) $(v LP_COUNT2)"
# without an administrator's spool
rm -f "$T"/spool/requests/*.req
chmod 500 "$T/spool/requests"
timeout 120 "$WINE" "$T/probe.exe" na 2>/dev/null | tr -d '\r' > "$T/o2"
chmod 700 "$T/spool/requests"
w() { sed -n "s/^$1 //p" "$T/o2" | head -1; }
[ "$(w RESTORE_NA)" = 80070005 ] && [ "$(w ENABLE_NA)" = 80070005 ] && pass "without an administrator's spool: access denied (E_ACCESSDENIED)" || fail "not elevated: $(cat "$T/o2" | tr '\n' ' ')"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
