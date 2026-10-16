#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# arp.exe (patches/sg/5520): -a lists the ARP table by interface as the
# kernel has it (ip neigh), -N and an address narrow it, -v shows the
# loopback, and -s / -d reach CreateIpNetEntry / DeleteIpNetEntry (a stand-in
# sg-netctl records the calls). It runs in an unprivileged network namespace
# of its own (unshare -rn) with a veth pair and a static neighbour.
#
#   WINE=/opt/wine-sg/bin/wine test/arp-gate.sh
# Mutants (programs/arp/main.c; x86_64_CFLAGS / i386_CFLAGS = "-g -O2 -DSG_MUTANT_X"):
# SG_MUTANT_ARP_NOTYPE (every entry "dynamic"), SG_MUTANT_ARP_ALLIFS (-N ignored).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINE WINESERVER

if [ -z "${SG_ARP_INNER:-}" ]; then
    for need in unshare ip; do
        command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }
    done
    [ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
    unshare -rn true 2>/dev/null || { echo "SKIP: no unprivileged network namespace"; exit 77; }
    SG_ARP_INNER=1 exec unshare -rn "$0" "$@"
fi

RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

T=$(mktemp -d /var/tmp/sg-arp.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM

ip link set lo up
ip link add t0 type veth peer name t1 2>/dev/null || { echo "SKIP: no veth in this kernel"; exit 77; }
ip addr add 10.9.0.1/24 dev t0; ip addr add 10.9.0.2/24 dev t1
ip addr add 10.8.0.1/24 dev t1
ip link set t0 up; ip link set t1 up
ip neigh add 10.9.0.77 lladdr 02:aa:bb:cc:dd:77 dev t0 nud permanent
ip neigh add 10.8.0.50 lladdr 02:aa:bb:cc:dd:50 dev t1 nud permanent
T0=$(ip -o link show t0 | cut -d: -f1); T1=$(ip -o link show t1 | cut -d: -f1)
HEX0=$(printf '%x' "$T0")

cat > "$T/fake-netctl" <<'EOS'
#!/bin/sh
{ printf 'CALL'; for a in "$@"; do printf ' [%s]' "$a"; done; printf '\n'; } >> "$FAKE_LOG"
case "$1" in
adapters) ip -o -4 addr | awk '{print $2}' | sort -u | while read -r d; do printf 'ADAPTER %s\nTYPE ethernet\nSTATE connected\nEND\n' "$d"; done; echo OK ;;
*) echo OK ;;
esac
EOS
chmod +x "$T/fake-netctl"

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
export SG_NETCTL="$T/fake-netctl" FAKE_LOG="$T/calls"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

arp() { : > "$T/calls"; DISPLAY= timeout -s KILL 60 "$WINE" arp "$@" > "$T/out" 2> "$T/err"; RET=$?; tr -d '\r' < "$T/out" > "$T/o"; tr -d '\r' < "$T/err" > "$T/e"; }

arp -a
grep -q "^Interface: 10.9.0.1 --- 0x$HEX0\$" "$T/o" && grep -q "^  10.9.0.77 *02-aa-bb-cc-dd-77 *static\$" "$T/o" \
    && pass "arp -a: the interface heading and the static entry with its hyphenated address" || fail "arp -a: $(cat "$T/o" "$T/e")"
grep -q "^  Internet Address      Physical Address      Type\$" "$T/o" && pass "the column heading" || fail "heading: $(cat "$T/o")"
grep -q "^Interface: 10.8.0.1\|^Interface: 10.9.0.2" "$T/o" && grep -q "10.8.0.50 *02-aa-bb-cc-dd-50 *static" "$T/o" \
    && pass "every interface that has entries is listed" || fail "second interface: $(cat "$T/o")"
! grep -q "127.0.0.1" "$T/o" && pass "the loopback is not shown" || fail "loopback shown"

arp -a 10.9.0.77
[ "$(grep -c "02-aa-bb-cc-dd" "$T/o")" = 1 ] && grep -q "10.9.0.77" "$T/o" && pass "arp -a ADDRESS shows that entry only" || fail "arp -a addr: $(cat "$T/o")"
arp -a -N 10.9.0.1
grep -q "10.9.0.77" "$T/o" && ! grep -q "10.8.0.50" "$T/o" && pass "-N narrows to one interface" || fail "-N: $(cat "$T/o")"
arp -a 10.200.0.1
grep -q "No ARP Entries Found" "$T/o" && pass "an address with no entry: No ARP Entries Found." || fail "none: $(cat "$T/o")"
arp -a -v
grep -q "^Interface: 127.0.0.1" "$T/o" && pass "-v shows the loopback interface" || fail "-v: $(cat "$T/o")"

arp -s 10.9.0.80 00-11-22-33-44-55
grep -q "CALL \[neigh\] \[add\] \[t0\] \[10.9.0.80\] \[00:11:22:33:44:55\] \[--permanent\]" "$T/calls" && [ $RET = 0 ] \
    && pass "arp -s asks for a permanent entry on the interface whose subnet holds the address" || fail "arp -s ($RET): $(cat "$T/calls" "$T/e")"
arp -s 10.9.0.81 00-11-22-33-44-56 10.9.0.1
grep -q "CALL \[neigh\] \[add\] \[t0\] \[10.9.0.81\]" "$T/calls" && pass "-s with an interface address uses that interface" || fail "-s if: $(cat "$T/calls")"
arp -d 10.9.0.77
grep -q "CALL \[neigh\] \[del\] \[t0\] \[10.9.0.77\]" "$T/calls" && [ $RET = 0 ] && pass "arp -d ADDRESS deletes the entry" || fail "arp -d ($RET): $(cat "$T/calls" "$T/e")"
arp -d 10.9.0.99
[ $RET = 1 ] && grep -q "specified entry was not found" "$T/e" && pass "deleting an address with no entry fails" || fail "arp -d none ($RET): $(cat "$T/e")"
arp -d '*'
grep -q "\[del\] \[t1\] \[10.8.0.50\]" "$T/calls" && grep -q "\[del\] \[t0\] \[10.9.0.77\]" "$T/calls" && pass "arp -d * deletes every entry" || fail "arp -d *: $(cat "$T/calls")"

arp -s 10.9.0.80 nonsense
[ $RET = 1 ] && pass "a bad physical address is refused" || fail "bad mac ($RET)"
arp
[ $RET = 1 ] && grep -q "ARP -s inet_addr" "$T/o" && pass "no arguments: usage, exit 1" || fail "usage ($RET)"

[ "$RC" = 0 ] && echo "arp-gate: all passed" || echo "arp-gate: FAILURES"
exit $RC
