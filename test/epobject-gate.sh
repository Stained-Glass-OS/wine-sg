#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# rpcrt4 (6117): a client binds "<object>@ncalrpc:" with no endpoint, the server registered its
# dynamic ncalrpc endpoint with RpcEpRegister under that object UUID. NVDA's in-process helper
# talks to nvda.exe this way; RpcEpResolveBinding asked the endpoint mapper for the nil object
# only and found nothing, so the helper never registered with NVDA. widl-generated client and
# server in two processes (test/epobject-probe.c, test/epobject.idl).
#   WINE=/path/to/build/wine test/epobject-gate.sh   (WIDL=... when not beside $WINE)
# Mutant: SG_MUTANT_EPMAP_NIL_OBJECT (dlls/rpcrt4/rpc_epmap.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
WIDL="${WIDL:-$(dirname "$WINE")/tools/widl/widl}"
[ -x "$WIDL" ] || WIDL="$(dirname "$WINE")/widl"
[ -x "$WIDL" ] || { echo "SKIP: no widl"; exit 77; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw missing"; exit 77; }
T=$(mktemp -d /var/tmp/sg-epobject.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
SRV=""
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
cp "$HERE/epobject.idl" "$T/"
(cd "$T" && "$WIDL" --win64 -Oif --prefix-server=s_ -h -H epobject.h -o /dev/null epobject.idl \
    && "$WIDL" --win64 -Oif --prefix-server=s_ -c -o epobject_c.c epobject.idl \
    && "$WIDL" --win64 -Oif --prefix-server=s_ -s -o epobject_s.c epobject.idl) \
    || { echo "FAIL  widl did not run"; exit 1; }
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -I"$T" -o "$T/probe.exe" "$HERE/epobject-probe.c" \
    "$T/epobject_c.c" "$T/epobject_s.c" -lrpcrt4 || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
(cd "$T" && exec timeout -s KILL 120 "$WINE" "$T/probe.exe" server >"$T/server.out" 2>&1 </dev/null) &
SRV=$!
n=0
until grep -q READY "$T/server.out" 2>/dev/null || [ $n -ge 60 ]; do sleep 1; n=$((n + 1)); done
grep -q READY "$T/server.out" || { echo "FAIL  the server did not start"; cat "$T/server.out"; exit 1; }
out=$(cd "$T" && timeout -s KILL 100 "$WINE" "$T/probe.exe" client 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
printf '%s\n' "$out" | grep -qx 'RESOLVE 0' && echo "PASS  the endpoint registered under the bound object is found" || { echo "FAIL  RpcEpResolveBinding: $(printf '%s' "$out" | grep RESOLVE)"; RC=1; }
printf '%s\n' "$out" | grep -qx 'PING 42' && echo "PASS  the call through it reaches the server" || { echo "FAIL  the call: $(printf '%s' "$out" | grep -E 'PING|EXCEPTION')"; RC=1; }
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
