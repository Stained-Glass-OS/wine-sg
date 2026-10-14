#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# nsiproxy.sys (patches/sg/5501): the IP helper tables and statistics a Windows
# program reads (GetIpStatisticsEx, GetIcmpStatisticsEx, GetTcpStatisticsEx,
# GetUdpStatisticsEx, GetIpNetTable2, GetIpForwardTable2), 64- and 32-bit, against
# what the kernel says (ip, /proc/net/snmp). It runs in an unprivileged network
# namespace of its own (unshare -rn) with a veth pair, a static ARP entry and
# routes, so nothing of the host's network is touched or needed.
# The IPv6 neighbour table (new in 5501) is read from a kernel dump: this
# test/nsi-neigh6-dump.py writes the real wire format and SG_NSI_NEIGH6_DUMP
# hands it to the driver, because a kernel may have no IPv6 to make one.
#
#   WINE=/opt/wine-sg/bin/wine test/nsi-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (nsiproxy.sys ip.c; build with x86_64_CFLAGS / unix: see notes/5501.txt):
# SG_MUTANT_NEIGH6_NONE, SG_MUTANT_NEIGH6_STATE, SG_MUTANT_NEIGH6_FAMILY,
# SG_MUTANT_NEIGH6_ROUTER.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINE WINESERVER

if [ -z "${SG_NSI_INNER:-}" ]; then
    for need in unshare ip python3 x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
        command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }
    done
    [ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
    unshare -rn true 2>/dev/null || { echo "SKIP: no unprivileged network namespace"; exit 77; }
    SG_NSI_INNER=1 exec unshare -rn "$0" "$@"
fi

RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

T=$(mktemp -d /var/tmp/sg-nsi.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM

# the namespace: lo, a veth pair with addresses, a static neighbour, a default route
ip link set lo up
ip link add t0 type veth peer name t1 2>/dev/null || { echo "SKIP: no veth in this kernel"; exit 77; }
ip addr add 10.9.0.1/24 dev t0; ip addr add 10.9.0.2/24 dev t1
ip link set t0 up; ip link set t1 up
ip neigh add 10.9.0.77 lladdr 02:aa:bb:cc:dd:77 dev t0 nud permanent
ip route add default via 10.9.0.254 dev t0
echo 0 > /proc/sys/net/ipv4/ip_forward 2>/dev/null
echo "0 2147483647" > /proc/sys/net/ipv4/ping_group_range 2>/dev/null
T0=$(ip -o link show t0 | cut -d: -f1); T1=$(ip -o link show t1 | cut -d: -f1)

for a in 64:x86_64 32:i686; do
    bits=${a%%:*}; cc=${a##*:}-w64-mingw32-gcc
    "$cc" -O2 -o "$T/nsi$bits.exe" "$HERE/nsi-probe.c" -liphlpapi -lws2_32 ||
        { echo "FAIL  the $bits-bit probe did not build"; exit 1; }
done
python3 -I "$HERE/nsi-neigh6-dump.py" "$T/dump.bin" 1 || { echo "FAIL  no dump"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# snmp NAME FIELD: a counter of /proc/net/snmp ("Ip:", "InReceives")
snmp() {
    awk -v p="$1:" -v f="$2" '$1 == p { if (!h) { for (i = 1; i <= NF; i++) n[$i] = i; h = 1 } else { print $(n[f]); exit } }' /proc/net/snmp
}
has() { printf '%s\n' "$1" | grep -qxF -- "$2"; }

for bits in 64 32; do
    probe="$T/nsi$bits.exe"
    run() { timeout -s KILL 120 "$WINE" "$probe" "$1" 2>/dev/null | tr -d '\r'; }

    # --- statistics against the kernel's counters, before/after the probe's traffic
    before_ip=$(snmp Ip InReceives); before_out=$(snmp Ip OutRequests); before_del=$(snmp Ip InDelivers)
    before_ao=$(snmp Tcp ActiveOpens); before_po=$(snmp Tcp PassiveOpens)
    before_ti=$(snmp Tcp InSegs); before_to=$(snmp Tcp OutSegs)
    before_ud=$(snmp Udp InDatagrams); before_uo=$(snmp Udp OutDatagrams)
    before_im=$(snmp Icmp InMsgs); before_om=$(snmp Icmp OutMsgs)
    out=$(run stats)
    after_ip=$(snmp Ip InReceives); after_out=$(snmp Ip OutRequests); after_del=$(snmp Ip InDelivers)
    after_ao=$(snmp Tcp ActiveOpens); after_po=$(snmp Tcp PassiveOpens)
    after_ti=$(snmp Tcp InSegs); after_to=$(snmp Tcp OutSegs)
    after_ud=$(snmp Udp InDatagrams); after_uo=$(snmp Udp OutDatagrams)
    after_im=$(snmp Icmp InMsgs); after_om=$(snmp Icmp OutMsgs)
    val() { printf '%s\n' "$out" | sed -n "s/^$1=//p" | head -1; }
    between() { # name key before after
        v=$(val "$2")
        if [ -n "$v" ] && [ "$v" -ge "$3" ] && [ "$v" -le "$4" ]; then pass "$bits-bit $1 = $v (kernel $3..$4)"
        else fail "$bits-bit $1 = '${v:-none}' (kernel $3..$4)"; fi
    }
    [ "$(val ip.ret)" = 0 ] && pass "$bits-bit GetIpStatisticsEx succeeds" || fail "$bits-bit GetIpStatisticsEx: $(val ip.ret)"
    fwd=$(cat /proc/sys/net/ipv4/ip_forward); want=$([ "$fwd" = 1 ] && echo 1 || echo 2)
    [ "$(val ip.forwarding)" = "$want" ] && pass "$bits-bit forwarding $want (ip_forward=$fwd)" || fail "$bits-bit forwarding $(val ip.forwarding), want $want"
    ttl=$(cat /proc/sys/net/ipv4/ip_default_ttl)
    [ "$(val ip.defaultttl)" = "$ttl" ] && pass "$bits-bit default TTL $ttl" || fail "$bits-bit default TTL $(val ip.defaultttl), want $ttl"
    between "IP InReceives" ip.inreceives "$before_ip" "$after_ip"
    between "IP InDelivers" ip.indelivers "$before_del" "$after_del"
    between "IP OutRequests" ip.outrequests "$before_out" "$after_out"
    between "TCP ActiveOpens" tcp.activeopens "$before_ao" "$after_ao"
    between "TCP PassiveOpens" tcp.passiveopens "$before_po" "$after_po"
    between "TCP InSegs" tcp.insegs "$before_ti" "$after_ti"
    between "TCP OutSegs" tcp.outsegs "$before_to" "$after_to"
    between "UDP InDatagrams" udp.indatagrams "$before_ud" "$after_ud"
    between "UDP OutDatagrams" udp.outdatagrams "$before_uo" "$after_uo"
    between "ICMP InMsgs" icmp.inmsgs "$before_im" "$after_im"
    between "ICMP OutMsgs" icmp.outmsgs "$before_om" "$after_om"
    [ "$(val icmp.echo)" -ge 1 ] 2>/dev/null && pass "$bits-bit an echo to 127.0.0.1 is answered" || fail "$bits-bit echo: '$(val icmp.echo)'"
    nif=$(ip -o link show | wc -l); naddr=$(ip -4 -o addr | wc -l)
    [ "$(val ip.numif)" = "$nif" ] && pass "$bits-bit interfaces $nif" || fail "$bits-bit interfaces $(val ip.numif), want $nif"
    [ "$(val ip.numaddr)" = "$naddr" ] && pass "$bits-bit addresses $naddr" || fail "$bits-bit addresses $(val ip.numaddr), want $naddr"

    # --- IPv4 neighbours and routes
    out=$(run neigh4)
    has "$out" "n4 10.9.0.77 if=$T0 state=6 router=0 unreach=0 mac=02:aa:bb:cc:dd:77" &&
        pass "$bits-bit the static ARP entry (permanent, 02:aa:bb:cc:dd:77 on t0)" || fail "$bits-bit neigh4: $out"
    out=$(run routes4)
    has "$out" "r4 0.0.0.0/0 via 10.9.0.254 if=$T0 metric=0" && pass "$bits-bit the default route via 10.9.0.254" || fail "$bits-bit routes: $out"
    has "$out" "r4 10.9.0.0/24 via 0.0.0.0 if=$T0 metric=0" && pass "$bits-bit the on-link route of t0" || fail "$bits-bit routes: $out"
    has "$out" "r4 10.9.0.0/24 via 0.0.0.0 if=$T1 metric=0" && pass "$bits-bit the on-link route of t1" || fail "$bits-bit routes: $out"

    # --- IPv6 neighbours, from the kernel-format dump
    "$WINESERVER" -k 2>/dev/null; sleep 1
    out=$(SG_NSI_NEIGH6_DUMP="$T/dump.bin" run neigh6)
    for want in \
        "n6 fe80::1 if=1 state=6 router=0 unreach=0 mac=00:11:22:33:44:55" \
        "n6 fe80::2 if=1 state=5 router=0 unreach=0 mac=02:00:00:00:00:02" \
        "n6 fe80::3 if=1 state=4 router=1 unreach=0 mac=02:00:00:00:00:03" \
        "n6 fe80::4 if=1 state=1 router=0 unreach=0 mac=" \
        "n6 fe80::5 if=1 state=0 router=0 unreach=1 mac=" \
        "n6 fe80::6 if=1 state=3 router=0 unreach=0 mac=02:00:00:00:00:06" \
        "n6 fe80::7 if=1 state=2 router=0 unreach=0 mac=02:00:00:00:00:07" \
        "n6 fe80::8 if=1 state=6 router=0 unreach=0 mac=02:00:00:00:00:08"; do
        has "$out" "$want" && pass "$bits-bit ${want%% mac=*}" || fail "$bits-bit missing: $want"
    done
    n=$(printf '%s\n' "$out" | grep -c '^n6 ')
    [ "$n" = 8 ] && pass "$bits-bit exactly 8 IPv6 neighbours (the AF_INET one is not reported)" || fail "$bits-bit $n IPv6 neighbours"
    "$WINESERVER" -k 2>/dev/null; sleep 1
done

[ $RC = 0 ] && echo "nsi-gate: all passed" || echo "nsi-gate: FAILURES"
exit $RC
