#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# iphlpapi lookups, small tables, initialisers, IP error strings, compartments,
# owner modules and SendARP (patches/sg/2406), 64- and 32-bit: the probe
# (test/iphlpapient-probe.c) checks GetIpForwardEntry2, GetIpNetEntry2,
# Get{Multicast,Anycast}IpAddressEntry, GetMulticastIpAddressTable,
# GetIpPathTable/Entry, Get{,Inverted}IfStackTable, Initialize{IpForward,
# UnicastIpAddress}Entry, GetIpErrorString, Get/SetSessionCompartmentId,
# GetOwnerModuleFrom{Tcp,Tcp6,Udp,Udp6}Entry, ...PidAndInfo and SendARP,
# all of which were unimplemented.
#
#   WINE=/opt/wine-sg/bin/wine test/iphlpapient-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (iphlpapi/entries.c): SG_MUTANT_IPE_IFACE, _NOIFACE, _MCAST, _STACK,
# _INIT, _ERRSTR, _COMP, _OWNER, _PATH, _ARP.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-iphlpapient.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/iphlpapient-probe.c" -liphlpapi -lws2_32 -luser32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
for a in x86_64 i686; do
    echo "== $a"
    # each run starts from a fresh profile: the probe changes the schemes
    "$WINESERVER" -k 2>/dev/null; rm -rf "$WINEPREFIX"; mkdir -p "$WINEPREFIX"
    timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
    "$WINESERVER" -w
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
