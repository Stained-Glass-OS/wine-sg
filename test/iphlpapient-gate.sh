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
# The multicast table is compared with the kernel's /proc/net/igmp{,6}, and with
# groups a helper process joins on lo while the probe runs.
# Mutants (iphlpapi/entries.c): SG_MUTANT_IPE_IFACE, _NOIFACE, _MCASTSYNTH, _MCASTSCOPE, _STACK,
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
JP=""
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$JP" ] && kill "$JP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/iphlpapient-probe.c" -liphlpapi -lws2_32 -luser32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
# a process that holds multicast memberships on lo for the probe to find
cat > "$T/join.py" <<'PYEOF'
import socket, struct, sys, time
r = []
try:
    s4 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s4.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, socket.inet_aton("239.77.88.99") + socket.inet_aton("127.0.0.1"))
    r.append("join4")
except OSError:
    pass
try:
    s6 = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
    s6.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_JOIN_GROUP, socket.inet_pton(socket.AF_INET6, "ff15::7788") + struct.pack("@I", 1))
    r.append("join6")
except OSError:
    pass
open(sys.argv[1], "w").write(",".join(r))
time.sleep(600)
PYEOF
python3 "$T/join.py" "$T/joined" & JP=$!
i=0; while [ ! -e "$T/joined" ] && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
JOINED=$(cat "$T/joined" 2>/dev/null)
echo "(helper joined: ${JOINED:-nothing})"
for a in x86_64 i686; do
    echo "== $a"
    # each run starts from a fresh profile: the probe changes the schemes
    "$WINESERVER" -k 2>/dev/null; rm -rf "$WINEPREFIX"; mkdir -p "$WINEPREFIX"
    timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
    "$WINESERVER" -w
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" "$JOINED" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
