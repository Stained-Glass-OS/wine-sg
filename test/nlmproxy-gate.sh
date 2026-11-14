#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Proxies for the network list manager's interfaces (wine-sg 7758): netprofm calls its event sinks
# through the global interface table, in their own apartments (SG 1689), which marshals each sink.
# INetworkCostManagerEvents (and the other interfaces that are not automation compatible) had no
# proxy: every Office start logged "CoMarshalInterface failed ... {dcb00009-...}" and cost events
# never reached their sinks. actxprxy now has them. test/nlmproxy-probe.c: a sink marshaled from an
# STA, called from an MTA, runs on its own thread; netprofm's Advise; INetworkListManager still
# crosses apartments.
#   WINE=/opt/wine-sg/bin/wine test/nlmproxy-gate.sh
# Before 7758 it fails at MarshalSink=0x80004002.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-nlmproxy.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/nlmproxy-probe.c" -lole32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
expect() {
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got: $(printf '%s' "$out" | tr '\n' ' ')"; fi
}
expect MarshalSink 0x00000000 "an INetworkCostManagerEvents sink marshals (it has a proxy)"
expect UnmarshalSink 0x00000000 "another apartment unmarshals it"
expect CallSink 0x00000000 "and calls it"
expect SinkCalls 1 "the sink is called once"
expect SinkOnOwnThread 1 "on its own (STA) thread"
expect Advise 0x00000000 "netprofm's connection point takes the sink"
expect MarshalNlm 0x00000000 "INetworkListManager still marshals"
expect GetConnectivity 0x00000000 "and is called from another apartment"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
