#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# hnetcfg (patch 3189): INetFwService/Services over the firewall's built-in groups, remote administration and
# ICMP settings, service restriction (its own rules), rule Interfaces, NAT event manager, 64- and 32-bit.
# The firewall is a stand-in status file and sg-admind spool; the host firewall is never touched.
#
#   WINE=/opt/wine-sg/bin/wine test/fasthnetcfg2-gate.sh
# Mutants (hnetcfg): SG_MUTANT_HN_SVC_NOGROUP (service.c), SG_MUTANT_HN_SVC_COUNT, SG_MUTANT_HN_RULE_PUSH (policy.c),
#   SG_MUTANT_HN_ICMP_NOPUSH (policy.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $cc >/dev/null || { echo "SKIP: $cc missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fasthn2.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
export SG_ADMIN_SPOOL="$T/spool" SG_FIREWALL_STATUS="$T/status"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || { chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"; }' EXIT INT TERM
mkdir -p "$T/spool/requests" "$T/spool/replies"
status() {
    printf 'state\trunning\nprofile\tdomain\ton\nprofile\tprivate\ton\nprofile\tpublic\ton\ncurrent\tprivate\n' > "$T/status"
    printf 'group\tssh\tall\tRemote access (SSH)\ngroup\tremote-desktop\tall\tRemote Desktop\ngroup\tfile-sharing\tdomain,private\tFile and Printer Sharing\ngroup\tnetwork-discovery\tnone\tNetwork Discovery\n' >> "$T/status"
}
status
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O2 -o "$T/probe-$a.exe" "$HERE/fasthnetcfg2-probe.c" -lole32 -loleaut32 -luuid -ladvapi32 || { fail "probe did not build ($a)"; exit 1; }
done

for a in x86_64 i686; do
    # a fresh prefix for each: the settings are kept in the registry
    "$WINESERVER" -k 2>/dev/null; sleep 4; rm -rf "$T/prefix"
    timeout -s KILL 300 "$WINE" wineboot -i >"$T/wb.$a" 2>&1; "$WINESERVER" -w
    rm -f "$T"/spool/requests/*.req
    status
    timeout 120 "$WINE" "$T/probe-$a.exe" 2>"$T/err.$a" | tr -d '\r' > "$T/o"
    v() { sed -n "s/^$1 //p" "$T/o" | head -1; }
    [ -n "${KEEP:-}" ] && cp "$T/o" "$T/o.$a"
    grep -q "^DONE" "$T/o" || { fail "the probe did not finish ($a): $(tail -2 "$T/o" | tr '\n' ' ')"; continue; }
    [ "$(v D_COUNT)" = "00000000 3" ] && [ "$(v S_COUNT)" = "00000000 3" ] && pass "the Services collection has the three services ($a)" || fail "count ($a): $(v D_COUNT) $(v S_COUNT)"
    [ "$(v D_ITEM0)" = "File and Printer Sharing|0|1|0" ] && [ "$(v D_ITEM1)" = "UPnP Framework|1|0|1" ] && [ "$(v D_ITEM2)" = "Remote Desktop|2|1|0" ] \
        && pass "names, types, Enabled from the firewall's groups, Customized against the defaults (domain) ($a)" || fail "domain items ($a): $(grep '^D_ITEM' "$T/o" | tr '\n' ' ')"
    [ "$(v S_ITEM0)" = "File and Printer Sharing|0|1|0" ] && [ "$(v S_ITEM1)" = "UPnP Framework|1|0|1" ] && [ "$(v S_ITEM2)" = "Remote Desktop|2|1|0" ] \
        && pass "the Standard profile follows the Private network ($a)" || fail "standard items ($a): $(grep '^S_ITEM' "$T/o" | tr '\n' ' ')"
    [ "$(v D_ITEM3)" = 80070057 ] && [ "$(v S_ITEMNEG)" = 80070057 ] && pass "an unknown service type is E_INVALIDARG ($a)" || fail "bad item ($a): $(v D_ITEM3) $(v S_ITEMNEG)"
    [ "$(v D_ENUM)" = "00000000 3" ] && pass "For Each lists the three services ($a)" || fail "enum ($a): $(v D_ENUM)"
    [ "$(v SC_DEF)" = "2 0 *" ] && [ "$(v SC_BADSCOPE)" = 80070057 ] && [ "$(v SC_BADIPV)" = 80070057 ] && [ "$(v SC_EMPTYADDR)" = 80070057 ] \
        && pass "scope defaults (any version, all, *); bad scope/version/address refused ($a)" || fail "scope defaults ($a): $(grep '^SC_DEF\|^SC_BAD\|^SC_EMPTY' "$T/o" | tr '\n' ' ')"
    [ "$(v SC_SUBNET_ADDR)" = LocalSubnet ] && [ "$(v SC_CUSTOM_GET)" = "2 10.0.0.0/8,192.168.1.5" ] && [ "$(v SC_ALL_ADDR)" = "*" ] && [ "$(v SC_V4_GET)" = 0 ] \
        && pass "Scope and RemoteAddresses follow each other; IpVersion is kept ($a)" || fail "scope sets ($a): $(grep '^SC_' "$T/o" | tr '\n' ' ')"
    [ "$(v SC_CUSTOMIZED)" = 1 ] && [ "$(v SC_DOMAIN_IPV)" = 2 ] && pass "a changed service is Customized; the Domain profile keeps its own ($a)" || fail "customized ($a): $(v SC_CUSTOMIZED) $(v SC_DOMAIN_IPV)"
    [ "$(v PT_COUNT)" = 4 ] && [ "$(v PT_445)" = "00000000 SMB over TCP|445|6" ] && [ "$(v PT_445UDP)" = 80070002 ] && [ "$(v PT_BAD)" = 80070057 ] && [ "$(v PT_ENUM)" = 4 ] \
        && pass "GloballyOpenPorts: the service's four ports, by port and protocol ($a)" || fail "ports ($a): $(grep '^PT_' "$T/o" | tr '\n' ' ')"
    [ "$(v DISP_IDS)" = "00000000 6" ] && [ "$(v DISP_TI)" = 1 ] && [ "$(v DISP_NAME)" = "00000000 File and Printer Sharing" ] \
        && pass "IDispatch works (names, type info, Invoke) ($a)" || fail "dispatch ($a): $(grep '^DISP_' "$T/o" | tr '\n' ' ')"
    reqs=$(for f in "$T"/spool/requests/*.req; do [ -f "$f" ] && { tr '\n' ' ' < "$f"; echo; }; done)
    printf '%s\n' "$reqs" | grep -qx 'firewall group file-sharing domain ' && printf '%s\n' "$reqs" | grep -qx 'firewall group remote-desktop domain,public ' \
        && printf '%s\n' "$reqs" | grep -qx 'firewall group network-discovery private ' \
        && pass "Enabled goes to sg-admind: 'firewall group NAME PROFILES' for the Private network ($a)" || fail "group requests ($a): $reqs"
    [ "$(v RA_DEF)" = "0 0 2" ] && [ "$(v RA_SET)" = 00000000 ] && [ "$(v RA_BADSCOPE)" = 80070057 ] && [ "$(v RA_BACK)" = "1 1 LocalSubnet" ] && [ "$(v RA_DOMAIN)" = 0 ] \
        && pass "RemoteAdminSettings: defaults, round trip (Standard profile only), bad scope refused ($a)" || fail "remote admin ($a): $(grep '^RA_' "$T/o" | tr '\n' ' ')"
    [ "$(v IC_ECHO_DEF)" = 0 ] && [ "$(v IC_BACK)" = "1 1 1 0 0" ] && [ "$(v IC_DOMAIN)" = 0 ] \
        && pass "IcmpSettings: defaults, round trip of the settings, other profile untouched ($a)" || fail "icmp ($a): $(grep '^IC_' "$T/o" | tr '\n' ' ')"
    printf '%s\n' "$reqs" | grep -qx 'firewall registry-push setting:private:IcmpSettings\\AllowInboundEchoRequest 1 ' \
        && pass "AllowInboundEchoRequest is pushed to the firewall at once ($a)" || fail "icmp push ($a): $reqs"
    [ "$(v SR_GET)" = 00000000 ] && [ "$(v SR_DEF)" = 0 ] && [ "$(v SR_ON)" = 00000000 ] && [ "$(v SR_ON_GET)" = 1 ] && [ "$(v SR_OTHER)" = 0 ] && [ "$(v SR_BAD)" = 80070057 ] \
        && [ "$(v SR_OFF)" = 00000000 ] && [ "$(v SR_OFF_GET)" = 0 ] && pass "ServiceRestriction: restrict, query per program, unrestrict, bad service refused ($a)" || fail "restriction ($a): $(grep '^SR_' "$T/o" | tr '\n' ' ')"
    [ "$(v SR_DISTINCT)" = 1 ] && [ "$(v SR_RULE_ADD)" = 00000000 ] && [ "$(v SR_RULE_COUNTS)" = "1 0" ] && [ "$(v SR_RULE_AGAIN)" = 1 ] && [ "$(v SR_RULE_REMOVED)" = 0 ] \
        && pass "the restriction's Rules are a collection of their own, apart from the policy's ($a)" || fail "restriction rules ($a): $(grep '^SR_' "$T/o" | tr '\n' ' ')"
    printf '%s\n' "$reqs" | grep -q 'Restricted svc rule' && fail "a service restriction's rule was given to the firewall ($a)" || pass "its rules are never given to the firewall ($a)"
    [ "$(v IF_DEF)" = 0 ] && [ "$(v IF_PUT)" = 00000000 ] && [ "$(v IF_GET)" = "8204 2 Ethernet Wi-Fi" ] && [ "$(v IF_BAD)" = 80070057 ] && [ "$(v IF_KEPT)" = 8204 ] && [ "$(v IF_CLEARED)" = 0 ] \
        && pass "a rule's Interfaces round trip as an array of names; a wrong type is refused; empty clears ($a)" || fail "interfaces ($a): $(grep '^IF_' "$T/o" | tr '\n' ' ')"
    [ "$(v NAT_DYN)" = "00000000 0" ] && pass "with no UPnP gateway the dynamic mappings collection is NULL and the call succeeds, as the static one ($a)" || fail "dynamic collection ($a): $(v NAT_DYN)"
    [ "$(v NAT_MGR)" = 00000000 ] && [ "$(v NAT_PUT_IP)" = 00000000 ] && [ "$(v NAT_REF1)" = 2 ] && [ "$(v NAT_REF2)" = 3 ] && [ "$(v NAT_REF3)" = 2 ] && [ "$(v NAT_REF4)" = 1 ] \
        && pass "the NAT event manager keeps its callbacks and releases them ($a)" || fail "nat events ($a): $(grep '^NAT_' "$T/o" | tr '\n' ' ')"

    # without an administrator's spool
    rm -f "$T"/spool/requests/*.req
    chmod 500 "$T/spool/requests"
    timeout 120 "$WINE" "$T/probe-$a.exe" na 2>/dev/null | tr -d '\r' > "$T/o2"
    chmod 700 "$T/spool/requests"
    w() { sed -n "s/^$1 //p" "$T/o2" | head -1; }
    [ "$(w NA_ENABLE)" = 80070005 ] && [ "$(w NA_STILL)" = 1 ] && pass "without an administrator's spool: Enabled is refused and nothing changes ($a)" || fail "not elevated ($a): $(tr '\n' ' ' < "$T/o2")"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
