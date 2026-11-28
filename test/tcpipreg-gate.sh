#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# The TCP/IP registry keys as Windows keeps them (patches/sg/9250-9253, ledger i0493, i0084).
#
# Windows' TCP/IP driver and DHCP client keep HKLM\System\CurrentControlSet\Services\Tcpip\Parameters\
# Interfaces\{adapter GUID} up to date (EnableDHCP, the fixed IPAddress/SubnetMask/DefaultGateway/
# NameServer, the lease's DhcpIPAddress/DhcpSubnetMask/DhcpServer/DhcpDefaultGateway/DhcpNameServer/
# DhcpDomain/Lease times), and each adapter's device: Control\Network\{4D36E972-...}\{GUID}\Connection
# (PnpInstanceID) -> Enum\<id> (ConfigFlags), Control\Class\{4D36E972-...}\NNNN (NetCfgInstanceId).
# dnspython (Electrum) reads exactly these. 9250: nsiproxy keeps them from GetAdaptersAddresses and the
# Linux side (NetworkManager's lease file, else the address's lifetime), and follows changes.
# 9251: GetAdaptersInfo/GetAdaptersAddresses report DHCP, the DHCP server and lease from them (was
# "DHCP enabled" for every adapter, no server). 9252: NV Hostname/NV Domain, SearchList. 9253: an adapter's GUID
# comes from its interface name (was the index), gone interfaces leave the table.
#
# In a private network and mount namespace of its own (dummy interfaces; /run a private tmpfs with a
# NetworkManager device file and systemd-resolved's resolv.conf the gate writes; the host's network and
# /run are never touched):
#   eth A: NetworkManager lease 10.77.0.5/24 from 10.77.0.1, name servers 10.77.0.53 10.77.0.54, domain
#     lan.example; eth B: fixed 192.168.77.9/24 via 192.168.77.1; eth D: an address with a lifetime and
#     no DHCP client record. System name servers behind resolved's stub: 10.77.0.53 10.77.0.54 192.168.77.53,
#     search corp.example lan.example.
#   1. keys as Windows writes them for A (DHCP), B (fixed), D (DHCP, the system's name servers), no key for
#      loopback; the device chain dnspython follows; Parameters SearchList/DhcpDomain/NV Hostname;
#      GetAdaptersInfo and GetAdaptersAddresses DHCP state; ipconfig /all
#   2. while Wine runs: a new interface appears (RegNotifyChangeKeyValue fires), A loses its lease
#      (link down), A gets a new lease, an interface goes away (its keys go); a change that changes
#      nothing writes nothing (no notification)
#   3. a standard user (shared prefix, $SG_USER1) reads the keys and may not write them; SYSTEM may
#      (needs the accounts of usersessions-gate.sh, else skipped)
#   4. dnspython (Windows Python), when $SG_REAL_HOME/.cache/sg-gates/tcpipreg has python-embed-amd64.zip
#      and a dnspython wheel (dnspython-*.whl), else skipped
#   WINE=... test/tcpipreg-gate.sh   (needs passwordless sudo; exit 77 otherwise)
#   5. .NET (Wine Mono): NetworkInterface IsDhcpEnabled and DhcpServerAddresses (skipped without Wine Mono)
#   6. (9210, i0510) each adapter's own name servers: GetAdaptersAddresses (IPv6 first), ipconfig /all,
#      .NET DnsAddresses -- not the stub's 127.0.0.53; a released adapter lists none.
#   8. (9462, i0690) GetPerAdapterInfo lists each adapter's own IPv4 name servers too (the system's only for
#      GetNetworkParams), and .NET's GetIPv4Properties works for every adapter with two or more of them:
#      that needs Wine Mono sg37 (mono-0035), whose IPv4InterfaceProperties allocates the size asked for --
#      sg36's fixed one-server structure was overrun (NullReferenceException). SG_MONO_DIR=<wine-mono-*>
#      runs the .NET sections on that Wine Mono (HKCU\Software\Wine\Mono RuntimePath).
#   7. (9211, i0511) eth A also has a NetworkManager DHCPv6 lease: Tcpip6\Parameters\Interfaces\{GUID}
#      Dhcpv6Iaid, Dhcpv6DNSServers, Dhcpv6DomainSearchList, Tcpip6\Parameters\Dhcpv6DUID; every adapter
#      has its Tcpip6 key; the lease gone, its values go
# Mutants: nsiproxy SG_MUTANT_TCPIPREG_NO_FOLLOW (tcpipreg.c), SG_MUTANT_TCPIPREG_NO_NM (dhcp.c),
#   SG_MUTANT_GUID_BY_INDEX (ndis.c, 9253);
#   iphlpapi SG_MUTANT_GAI_DHCP_ALWAYS (iphlpapi_main.c); dnsapi SG_MUTANT_DNS_SYSTEM_ONLY (query.c, 9210);
#   nsiproxy SG_MUTANT_TCPIP6_NO_DHCP6 (dhcp.c, 9211); iphlpapi SG_MUTANT_PERADAPTER_SYSTEM_SERVERS
#   (iphlpapi_main.c, 9462); Wine Mono -d:SG_MUTANT_PERADAPTER_FIXED_BUFFER (System.dll, mono-0035).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_SYSTEM=${SG_SYSTEM:-sgsystem}; SG_USER1=${SG_USER1:-sgtest1}; SG_GROUP=${SG_GROUP:-sgwine}
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: no mingw"; exit 77; }
sudo -n true 2>/dev/null || { echo "SKIP: needs passwordless sudo"; exit 77; }
command -v ip >/dev/null && command -v unshare >/dev/null && command -v setpriv >/dev/null || { echo "SKIP: needs ip, unshare, setpriv"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
W=$(mktemp -d /var/tmp/sg-tcpipreg.XXXXXX) || exit 1
chmod 755 "$W"
cleanup() { [ -n "${KEEP:-}" ] || sudo -n rm -rf "$W"; }
trap cleanup EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$W/probe.exe" "$HERE/tcpipreg-probe.c" -liphlpapi -ladvapi32 -lws2_32 || { echo "FAIL probe build"; exit 1; }
CACHE=${SG_REAL_HOME:-$HOME}/.cache/sg-gates/tcpipreg
MULTI=1
for u in "$SG_SYSTEM" "$SG_USER1"; do id "$u" >/dev/null 2>&1 || MULTI=0; done
getent group "$SG_GROUP" >/dev/null || MULTI=0
MCS=""
MONO_RT=${SG_MONO_DIR:-}
[ -n "$MONO_RT" ] && [ -f "$MONO_RT/lib/mono/4.5/mcs.exe" ] && MCS=$MONO_RT/lib/mono/4.5/mcs.exe
[ -n "$MCS" ] || for d in "$(dirname "$WINE")/../share/wine/mono" /opt/wine-sg/share/wine/mono /usr/share/wine/mono; do
    for m in "$d" "$d"/wine-mono-*; do [ -f "$m/lib/mono/4.5/mcs.exe" ] && { MCS=$m/lib/mono/4.5/mcs.exe; break 2; }; done
done
cp "$HERE/tcpipreg-net.cs" "$W/net.cs"

cat > "$W/inner.sh" <<'INNER'
# runs as root in the private network and mount namespace
set -u
W=$1; WINE=$2; WINESERVER=$3; U=$4; G=$5; N=$6; MULTI=$7; SG_SYSTEM=$8; SG_USER1=$9; SG_GROUP=${10}; CACHE=${11}; MCS=${12}; MONO_RT=${13}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
mount -t tmpfs -o mode=1777,size=1m tmpfs /tmp/.X11-unix || { echo "FAIL X11 tmpfs"; exit 1; }
mount -t tmpfs -o mode=755,size=4m tmpfs /run || { echo "FAIL /run tmpfs"; exit 1; }
mkdir -p /run/NetworkManager/devices /run/systemd/resolve
printf 'nameserver 127.0.0.53\nsearch corp.example lan.example\n' > "$W/stub-resolv.conf"
printf 'nameserver 10.77.0.53\nnameserver 10.77.0.54\nnameserver 192.168.77.53\nsearch corp.example lan.example\n' > /run/systemd/resolve/resolv.conf
R=$(readlink -f /etc/resolv.conf)
mkdir -p "$(dirname "$R")"; [ -e "$R" ] || : > "$R"
mount --bind "$W/stub-resolv.conf" "$R" || { echo "FAIL resolv.conf bind"; exit 1; }
ip link set lo up
idx() { ip -o link show "$1" | cut -d: -f1; }
nm_lease() { # nm_lease DEV SERVER DNS DOMAIN LEASE EXPIRY
    printf '[device]\nmanaged=true\nconnection-uuid=00000000-0000-0000-0000-00000000000%s\n\n[dhcp4]\ndhcp4.dhcp_lease_time=%s\ndhcp4.dhcp_server_identifier=%s\ndhcp4.domain_name_servers=%s\ndhcp4.domain_name=%s\ndhcp4.expiry=%s\n' \
        "$(idx "$1")" "$5" "$2" "$3" "$4" "$6" > "/run/NetworkManager/devices/$(idx "$1")"; }
nm_nolease() { printf '[device]\nmanaged=true\n' > "/run/NetworkManager/devices/$(idx "$1")"; }
ip link add sgeth0 type dummy && ip link add sgeth1 type dummy && ip link add sgeth3 type dummy || { echo "FAIL dummy interfaces"; exit 1; }
NOW=$(date +%s); EXP=$((NOW + 3000))
nm_lease sgeth0 10.77.0.1 "10.77.0.53 10.77.0.54" lan.example 3600 $EXP
# and a DHCPv6 lease (9211)
printf '\n[dhcp6]\ndhcp6.dhcp6_client_id=00:04:11:22:33:44:55:66:77:88:99:aa:bb:cc:dd:ee:ff:00\ndhcp6.dhcp6_name_servers=fd77::53\ndhcp6.dhcp6_domain_search=v6.example\ndhcp6.iaid=0x0a0b0c0d\ndhcp6.ip6_address=fd77::5\n' \
    >> "/run/NetworkManager/devices/$(idx sgeth0)"
nm_nolease sgeth1
ip link set sgeth0 up; ip link set sgeth1 up; ip link set sgeth3 up
ip addr add 10.77.0.5/24 dev sgeth0 valid_lft 3000 preferred_lft 3000
ip addr add 192.168.77.9/24 dev sgeth1
ip addr add 10.88.0.7/16 dev sgeth3 valid_lft 1800 preferred_lft 1800
ip route add default via 10.77.0.1 dev sgeth0 metric 100
ip route add default via 192.168.77.1 dev sgeth1 metric 200

asu() { local u=$1; shift; setpriv --reuid="$(id -u "$u")" --regid="$(id -g "$u")" --init-groups -- \
        env USER="$u" LOGNAME="$u" HOME="$W/home-$u" XDG_RUNTIME_DIR="$W/run-$u" "$@"; }
me() { setpriv --reuid="$U" --regid="$G" --init-groups -- env USER="$N" LOGNAME="$N" "$@"; }
export WINEPREFIX="$W/pfx" WINEDEBUG=-all WINESERVER
me mkdir -p "$WINEPREFIX"
me timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
me cp "$W/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
# a Wine process that stays: nsiproxy keeps running between the probes
me timeout -s KILL 900 "$WINE" 'C:\probe.exe' sleep 880000 > "$W/keeper.out" 2>/dev/null &
KEEPER=$!
dump() { me timeout -s KILL 60 "$WINE" 'C:\probe.exe' dump 2>/dev/null | tr -d '\r' > "$W/dump"; }
# until DUMP matches every pattern (up to 20 s)
await() { local t p ok; for t in $(seq 20); do dump; ok=1; for p in "$@"; do grep -qE -- "$p" "$W/dump" || { ok=0; break; }; done
          [ $ok = 1 ] && return 0; sleep 1; done; return 1; }
check() { local what=$1; shift; if await "$@"; then pass "$what"; else fail "$what"; for p in "$@"; do grep -qE -- "$p" "$W/dump" || echo "      missing: $p"; done; fi; }
absent() { grep -qE -- "$2" "$W/dump" && fail "$1" || pass "$1"; }
q() { printf '%s' "$1" | sed 's/[{}]/\\&/g'; }
# an adapter's GUID, as GetAdaptersAddresses names it (up to 20 s for a new interface)
guid() { local t g; for t in $(seq 20); do dump; g=$(sed -n "s/^ADAPTER \({[^ ]*}\) .* name $1 dhcpflag.*/\1/p" "$W/dump" | head -1)
         [ -n "$g" ] && { printf '%s' "$g"; return; }; sleep 1; done; printf '{none-%s}' "$1"; }
A=$(guid sgeth0); B=$(guid sgeth1); D=$(guid sgeth3); L=$(guid lo)
qA=$(q "$A"); qB=$(q "$B"); qD=$(q "$D")
case "$A$B$D" in *'{0000000'*) fail "adapter GUIDs are the interface index ($A $B $D)" ;; *none*) fail "no GUIDs ($A $B $D)" ;;
*) pass "adapter GUIDs come from the interface (not its index)" ;; esac

# 1. as written at start
check "DHCP adapter (NetworkManager lease): EnableDHCP, address, mask, server, lease, gateway, name servers, domain" \
    "^VAL $qA EnableDHCP dword 1$" "^VAL $qA DhcpIPAddress sz \[10\.77\.0\.5\]$" "^VAL $qA DhcpSubnetMask sz \[255\.255\.255\.0\]$" \
    "^VAL $qA DhcpServer sz \[10\.77\.0\.1\]$" "^VAL $qA Lease dword 3600$" "^VAL $qA LeaseTerminatesTime dword $EXP$" \
    "^VAL $qA LeaseObtainedTime dword $((EXP - 3600))$" "^VAL $qA DhcpDefaultGateway multi \[10\.77\.0\.1\]$" \
    "^VAL $qA DhcpNameServer sz \[10\.77\.0\.53 10\.77\.0\.54\]$" "^VAL $qA DhcpDomain sz \[lan\.example\]$" \
    "^VAL $qA NameServer sz \[\]$" "^VAL $qA Domain sz \[\]$"
absent "DHCP adapter: no fixed IPAddress" "^VAL $qA IPAddress "
check "fixed adapter: EnableDHCP 0, IPAddress, SubnetMask, DefaultGateway, NameServer = the system's servers" \
    "^VAL $qB EnableDHCP dword 0$" "^VAL $qB IPAddress multi \[192\.168\.77\.9\]$" "^VAL $qB SubnetMask multi \[255\.255\.255\.0\]$" \
    "^VAL $qB DefaultGateway multi \[192\.168\.77\.1\]$" "^VAL $qB NameServer sz \[10\.77\.0\.53,10\.77\.0\.54,192\.168\.77\.53\]$"
absent "fixed adapter: no lease values" "^VAL $qB Dhcp(IPAddress|Server|NameServer) "
check "leased address without a DHCP client record: DHCP from its lifetime, the system's name servers" \
    "^VAL $qD EnableDHCP dword 1$" "^VAL $qD DhcpIPAddress sz \[10\.88\.0\.7\]$" "^VAL $qD DhcpSubnetMask sz \[255\.255\.0\.0\]$" \
    "^VAL $qD Lease dword 1(79[0-9]|80[0-2])$" "^VAL $qD DhcpServer sz \[255\.255\.255\.255\]$" \
    "^VAL $qD DhcpNameServer sz \[10\.77\.0\.53 10\.77\.0\.54 192\.168\.77\.53\]$"
absent "no key for the loopback interface" "^KEY $(q "$L")$"
check "the device chain dnspython follows: Connection PnpInstanceID -> Enum ConfigFlags 0, Class NetCfgInstanceId" \
    "^PNP $qA ROOT\\\\NET\\\\[0-9]{4} flags 0 driver \{4d36e972-e325-11ce-bfc1-08002be10318\}\\\\[0-9]{4} conn \[sgeth0\]$" \
    "^PNP $qB ROOT\\\\NET\\\\[0-9]{4} flags 0 " "^CLASS [0-9]{4} $qA ROOT\\\\NET\\\\[0-9]{4}$" "^CLASS [0-9]{4} $qB "
check "Parameters: SearchList (beyond the lease's domain), DhcpDomain, NV Hostname" \
    "^PARAM SearchList sz \[corp\.example\]$" "^PARAM DhcpDomain sz \[lan\.example\]$" "^PARAM NV Hostname sz \[.+\]$" "^PARAM Hostname sz \[.+\]$"
check "GetAdaptersInfo: DhcpEnabled, DHCP server and lease times from the keys" \
    "^INFO $qA dhcp 1 server \[10\.77\.0\.1\] obtained $((EXP - 3600)) expires $EXP$" "^INFO $qB dhcp 0 "
check "GetAdaptersAddresses: IP_ADAPTER_DHCP_ENABLED for the DHCP adapters only" \
    "^ADAPTER $qA .* dhcpflag 1$" "^ADAPTER $qB .* dhcpflag 0$" "^ADAPTER $qD .* dhcpflag 1$"
# 7. the Tcpip6 keys (9211)
check "DHCPv6 lease: Tcpip6 Interfaces key with Dhcpv6Iaid, Dhcpv6DNSServers, Dhcpv6DomainSearchList; the machine's Dhcpv6DUID" \
    "^VAL6 $qA Dhcpv6Iaid dword 168496141$" "^VAL6 $qA Dhcpv6DNSServers binary \[fd770000000000000000000000000053\]$" \
    "^VAL6 $qA Dhcpv6DomainSearchList sz \[v6\.example\]$" "^PARAM6 Dhcpv6DUID binary \[0004112233445566778899aabbccddeeff00\]$" \
    "^KEY6 $qB$" "^KEY6 $qD$"
absent "no DHCPv6 lease: no DHCPv6 values" "^VAL6 ($qB|$qD) Dhcpv6"
# 6. each adapter's own name servers (9210)
check "GetAdaptersAddresses: each adapter's own name servers (the DHCPv6 lease's first), not the stub's" \
    "^DNS $qA \[fd77::53 10\.77\.0\.53 10\.77\.0\.54\]$" "^DNS $qB \[10\.77\.0\.53 10\.77\.0\.54 192\.168\.77\.53\]$" \
    "^DNS $qD \[10\.77\.0\.53 10\.77\.0\.54 192\.168\.77\.53\]$"
absent "no adapter lists the stub resolver (127.0.0.53), loopback neither" "^DNS .*127\.0\.0\.53"
# 8. GetPerAdapterInfo per adapter (9462)
check "GetPerAdapterInfo: each adapter's own IPv4 name servers (the lease's two for A, not the system's three)" \
    "^PERADAPTER $qA \[10\.77\.0\.53 10\.77\.0\.54\]$" "^PERADAPTER $qB \[10\.77\.0\.53 10\.77\.0\.54 192\.168\.77\.53\]$" \
    "^PERADAPTER $qD \[10\.77\.0\.53 10\.77\.0\.54 192\.168\.77\.53\]$"
absent "GetPerAdapterInfo: no adapter lists the stub resolver (127.0.0.53)" "^PERADAPTER .*127\.0\.0\.53"
me timeout -s KILL 60 "$WINE" ipconfig /all 2>/dev/null | tr -d '\r' > "$W/ipconfig"
if grep -A20 'sgeth0' "$W/ipconfig" | grep -qE 'DNS servers.*fd77::53' && grep -A20 'sgeth1' "$W/ipconfig" | grep -qE '192\.168\.77\.53' &&
   ! grep -q '127\.0\.0\.53' "$W/ipconfig"; then pass "ipconfig /all: DNS servers per adapter"
else fail "ipconfig /all: DNS servers per adapter"; grep -E 'adapter|DNS|^ +[0-9a-f.:]+$' "$W/ipconfig" | sed 's/^/      /' | head -30; fi
if grep -A12 'sgeth0' "$W/ipconfig" | grep -qE 'DHCP enabled.*Yes' && grep -A12 'sgeth0' "$W/ipconfig" | grep -qE 'DHCP server.*10\.77\.0\.1' &&
   grep -A12 'sgeth1' "$W/ipconfig" | grep -qE 'DHCP enabled.*No'; then pass "ipconfig /all: DHCP enabled and server per adapter"
else fail "ipconfig /all"; sed 's/^/      /' "$W/ipconfig" | head -60; fi

# 2. following changes while Wine runs
waitnote() { me timeout -s KILL 60 "$WINE" 'C:\probe.exe' wait "$1" 2>/dev/null | tr -d '\r' > "$W/wait.out" & WP=$!
             for t in $(seq 50); do grep -q ARMED "$W/wait.out" 2>/dev/null && return; sleep 0.2; done; }
waitnote 20000
ip link add sgeth2 type dummy; ip link set sgeth2 up; ip addr add 172.30.0.2/16 dev sgeth2
C=$(guid sgeth2); qC=$(q "$C")
wait $WP
grep -q NOTIFY "$W/wait.out" && pass "a new interface: RegNotifyChangeKeyValue on Interfaces fires" || fail "a new interface: no notification ($(cat "$W/wait.out"))"
check "a new interface: its keys, at once" "^VAL $qC EnableDHCP dword 0$" "^VAL $qC IPAddress multi \[172\.30\.0\.2\]$" \
    "^VAL $qC SubnetMask multi \[255\.255\.0\.0\]$" "^PNP $qC ROOT\\\\NET\\\\[0-9]{4} flags 0 "
nm_nolease sgeth0; ip addr flush dev sgeth0; ip link set sgeth0 down
check "the cable out (no address, no lease): released as Windows shows it" \
    "^VAL $qA EnableDHCP dword 1$" "^VAL $qA DhcpIPAddress sz \[0\.0\.0\.0\]$" "^VAL $qA DhcpServer sz \[255\.255\.255\.255\]$"
absent "released: no lease name servers" "^VAL $qA DhcpNameServer "
check "released: GetAdaptersAddresses lists no name servers for it" "^DNS $qA \[\]$"
check "released: GetPerAdapterInfo lists none for it either" "^PERADAPTER $qA \[\]$"
absent "the DHCPv6 lease gone: its Tcpip6 values go" "^VAL6 $qA Dhcpv6(Iaid|DNSServers|DomainSearchList) "
EXP2=$(( $(date +%s) + 7000 ))
nm_lease sgeth0 10.77.0.2 10.77.0.53 lan.example 7200 $EXP2
ip link set sgeth0 up; ip addr add 10.77.0.6/24 dev sgeth0 valid_lft 7000 preferred_lft 7000
check "a new lease: the new address, server, lease and name servers" \
    "^VAL $qA DhcpIPAddress sz \[10\.77\.0\.6\]$" "^VAL $qA DhcpServer sz \[10\.77\.0\.2\]$" "^VAL $qA Lease dword 7200$" \
    "^VAL $qA DhcpNameServer sz \[10\.77\.0\.53\]$" "^INFO $qA dhcp 1 server \[10\.77\.0\.2\] " \
    "^PERADAPTER $qA \[10\.77\.0\.53\]$"
ip link del sgeth2
sleep 3
dump
absent "an interface gone: its Interfaces key goes" "^KEY $qC$"
absent "an interface gone: its device and class keys go" "^(PNP|CLASS .*) $qC"
# made again after another interface took the next index: the same adapter, the same GUID and keys
ip link add sgeth9 type dummy; ip link add sgeth2 type dummy; ip link set sgeth2 up; ip addr add 172.30.0.2/16 dev sgeth2
check "an interface made again (another index) keeps its GUID: GetAdaptersAddresses and its keys" \
    "^ADAPTER $qC .* name sgeth2 " "^VAL $qC IPAddress multi \[172\.30\.0\.2\]$"
[ "$(grep -c ' name sgeth2 ' "$W/dump")" = 1 ] && pass "one adapter for it, not the old one beside it" || fail "sgeth2 listed $(grep -c ' name sgeth2 ' "$W/dump") times"
G9=$(q "$(guid sgeth9)")
ip link del sgeth9
for t in $(seq 20); do dump; grep -qE "^KEY $G9$" "$W/dump" || break; sleep 1; done
waitnote 6000
ip link set sgeth1 mtu 1400
wait $WP
grep -q QUIET "$W/wait.out" && pass "a change that changes no key writes nothing (no notification)" || fail "nothing changed, still a notification ($(cat "$W/wait.out"))"

# 4. dnspython, the Electrum consumer
if [ -f "$CACHE/python-embed-amd64.zip" ] && ls "$CACHE"/dnspython-*.whl >/dev/null 2>&1; then
    me mkdir -p "$W/py"
    me sh -c "cd '$W/py' && unzip -q -o '$CACHE/python-embed-amd64.zip' && unzip -q -o $(ls "$CACHE"/dnspython-*.whl | head -1)"
    for f in "$W"/py/python3*._pth; do me sh -c "echo . >> '$f'; echo 'import site' >> '$f'"; done
    printf 'import dns.resolver\nr = dns.resolver.Resolver()\nprint("NS", " ".join(r.nameservers))\nprint("SEARCH", " ".join(str(d) for d in r.search))\n' > "$W/py/ns.py"
    me timeout -s KILL 120 "$WINE" "$W/py/python.exe" "$W/py/ns.py" 2>"$W/py.err" | tr -d '\r' > "$W/py.out"
    if grep -qE '^NS .*10\.77\.0\.53' "$W/py.out" && grep -qE '^NS .*192\.168\.77\.53' "$W/py.out"; then
        pass "dnspython's Resolver() reads the name servers from the registry ($(grep ^NS "$W/py.out"))"
    else fail "dnspython: $(cat "$W/py.out") $(tail -3 "$W/py.err")"; fi
else
    echo "SKIP  dnspython (no $CACHE/python-embed-amd64.zip + dnspython-*.whl)"
fi
# 5. .NET
if [ -n "$MCS" ]; then
    [ -n "$MONO_RT" ] && me "$WINE" reg add 'HKCU\Software\Wine\Mono' /v RuntimePath /d "$(me "$WINE" winepath -w "$MONO_RT" | tr -d '\r')" /f >/dev/null 2>&1
    me sh -c "cd '$W' && timeout -s KILL 300 '$WINE' '$MCS' -out:net.exe net.cs" >/dev/null 2>&1
    me timeout -s KILL 120 "$WINE" "$W/net.exe" 2>/dev/null | tr -d '\r' > "$W/net.out"
    if grep -qE "^NET $qA dhcp 1 servers \[10\.77\.0\.2\]$" "$W/net.out" && grep -qE "^NET $qB dhcp 0 " "$W/net.out"; then
        pass ".NET NetworkInterface: IsDhcpEnabled and DhcpServerAddresses per adapter"
    else fail ".NET: $(tr '\n' ' ' < "$W/net.out")"; fi
    if grep -qE "^NETDNS $qA \[10\.77\.0\.53\]$" "$W/net.out" && grep -qE "^NETDNS $qB \[10\.77\.0\.53 10\.77\.0\.54 192\.168\.77\.53\]$" "$W/net.out"; then
        pass ".NET IPInterfaceProperties.DnsAddresses: each adapter's own"
    else fail ".NET DnsAddresses: $(grep NETDNS "$W/net.out" | tr '\n' ' ')"; fi
    # B and D have three name servers, A one: GetIPv4Properties of each, many times (the overrun was heap
    # corruption, not always at the first call)
    if grep -qE "^NETV4 rounds 200 ok [0-9]+ errors 0$" "$W/net.out" && ! grep -qE "^NETV4 rounds 200 ok 0 " "$W/net.out"; then
        pass ".NET GetIPv4Properties for adapters with several name servers, 200 rounds ($(grep ^NETV4 "$W/net.out"))"
    else fail ".NET GetIPv4Properties with several name servers: '$(grep -E '^NETV4' "$W/net.out")' $(grep -c . "$W/net.out") lines (Wine Mono sg36: NullReferenceException)"; fi
else
    echo "SKIP  .NET (no Wine Mono)"
fi
kill $KEEPER 2>/dev/null
me "$WINESERVER" -k 2>/dev/null

# 3. a standard user in the shared prefix
if [ "$MULTI" = 1 ]; then
    P=$W/shared
    for u in "$SG_SYSTEM" "$SG_USER1"; do install -d -o "$u" -m 700 "$W/home-$u" "$W/run-$u"; done
    install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$P"
    asu "$SG_SYSTEM" touch "$P/.sg-system-prefix"
    sys() { asu "$SG_SYSTEM" env SG_WINSTATION='__wineservice_winstation\Default' WINEPREFIX="$P" "$@"; }
    sys sh -c "umask 002; '$WINESERVER' -p"
    sys sh -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
    cp "$W/probe.exe" "$W/sprobe.exe"; chmod 755 "$W/sprobe.exe"
    for t in $(seq 20); do
        sys sh -c "umask 002; timeout -s KILL 60 '$WINE' '$W/sprobe.exe' dump" 2>/dev/null | tr -d '\r' > "$W/sdump"
        grep -qE "^VAL $qA EnableDHCP dword 1$" "$W/sdump" && break; sleep 1
    done
    grep -qE "^VAL $qA EnableDHCP dword 1$" "$W/sdump" && pass "shared prefix: SYSTEM's nsiproxy wrote the keys" || fail "shared prefix: no keys ($(head -3 "$W/sdump"))"
    asu "$SG_USER1" env WINEPREFIX="$P" timeout -s KILL 120 "$WINE" "$W/sprobe.exe" write 2>/dev/null | tr -d '\r' > "$W/uw"
    if grep -q '^READ ok' "$W/uw" && grep -q '^WRITE denied 5' "$W/uw" && grep -q '^CREATE denied 5' "$W/uw"; then
        pass "a standard user reads the keys, may not change or add to them (ERROR_ACCESS_DENIED)"
    else fail "standard user: $(tr '\n' ' ' < "$W/uw")"; fi
    sys sh -c "umask 002; timeout -s KILL 60 '$WINE' '$W/sprobe.exe' write" 2>/dev/null | tr -d '\r' > "$W/sw"
    grep -q '^WRITE ok' "$W/sw" && grep -q '^CREATE ok' "$W/sw" && pass "SYSTEM may write them" || fail "SYSTEM: $(tr '\n' ' ' < "$W/sw")"
    sys "$WINESERVER" -k 2>/dev/null
else
    echo "SKIP  standard user (no $SG_SYSTEM/$SG_USER1/$SG_GROUP)"
fi
exit $((fails > 0))
INNER

sudo -n -E unshare -n -m --propagation private -- bash "$W/inner.sh" "$W" "$WINE" "$WINESERVER" "$(id -u)" "$(id -g)" "$(id -un)" \
    "$MULTI" "$SG_SYSTEM" "$SG_USER1" "$SG_GROUP" "$CACHE" "$MCS" "$MONO_RT"
rc=$?
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
