#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# IP interface table and change notifications (patches/sg/1623):
# test/ipnotify-probe.c, 64-bit, in a user and network namespace of its own
# (unshare -rn), where the gate may add an address, a route and a link.
# GetIpInterfaceTable failed (ERROR_NOT_SUPPORTED); NotifyIpInterfaceChange
# and NotifyRouteChange2 never called back, NotifyUnicastIpAddressChange
# only gave its initial notification (Chromium, .NET and Teams watch the
# network with these).
#
#   WINE=/opt/wine-sg/bin/wine test/ipnotify-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_MIB_NOTIFY, SG_MUTANT_NO_IP_INTERFACES
# (iphlpapi/iphlpapi_main.c), SG_MUTANT_NO_LINK_ROUTE_EVENTS
# (nsiproxy.sys/nsi.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
command -v ip >/dev/null || { echo "SKIP: ip (iproute2) not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unshare -rn true 2>/dev/null || { echo "SKIP: no unprivileged network namespaces"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ipnotify.XXXXXX)
trap 'rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/ipnotify-probe.c" -liphlpapi -lws2_32 ||
    { echo "FAIL  probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER WINE T
unshare -rn sh -c '
    ip link set lo up
    mkdir -p "$WINEPREFIX"
    timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
    "$WINESERVER" -w
    (cd "$T" && timeout -s KILL 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null </dev/null > "$T/raw") &
    i=0; while ! grep -q "^READY" "$T/raw" 2>/dev/null && [ $i -lt 240 ]; do sleep 0.5; i=$((i + 1)); done
    sleep 1
    ip addr add 10.9.8.7/24 dev lo
    ip route add 10.77.0.0/16 dev lo
    ip link add sgdummy0 type dummy
    wait
    "$WINESERVER" -k 2>/dev/null
'
tr -d '\r' < "$T/raw" > "$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
