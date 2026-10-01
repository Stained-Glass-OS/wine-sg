#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# IP_MULTICAST_IF takes an interface index (patches/sg/0639), as Windows does:
# a value in 0.x.x.x (not 0.0.0.0) is an index in network byte order.
# Zeroconf's mDNS passes it so; Wine handed it to Linux as an address,
# "Invalid argument", and DYMO Connect's web service (its printer discovery)
# stopped -- the service athenaNet and other web apps print labels through.
#
#   WINE=/opt/wine-sg/bin/wine test/mcastif-gate.sh
# Mutation: build with -DSG_MUTANT_NOIFINDEX (the index goes to Linux as an
# address again): byindex fails.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-mcastif.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O2 -o "$T/mc.exe" "$HERE/mcastif-probe.c" -lws2_32 -liphlpapi || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout 60 "$WINE" "$T/mc.exe" 2>/dev/null | tr -d '\r')
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
[ -n "$(v index)" ] && [ "$(v index)" != 0 ] || fail "no loopback interface index: $out"
[ "$(v byindex)" = 0 ] && pass "IP_MULTICAST_IF by interface index (0.0.0.$(v index)) is accepted" || fail "by index: $(v byindex)"
[ "$(v byaddress)" = 0 ] && pass "by address (127.0.0.1), as before" || fail "by address: $(v byaddress)"
[ "$(v nosuch)" != 0 ] && pass "an index no interface has is refused ($(v nosuch))" || fail "a missing interface was accepted"
exit $RC
