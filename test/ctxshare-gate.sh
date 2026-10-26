#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Calls on one RPC context handle (patches/sg/6123): a serialized context handle is a reader/writer
# lock, as on Windows -- [in]-only calls share it, an [in, out] call has it alone. Every call took it
# exclusively, and CCleaner's uninstaller (Icarus: a call holding the handle waited for a second
# call on the same handle) hung at "Starting setup". widl client+server: test/ctxshare.idl,
# test/ctxshare-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/ctxshare-gate.sh
# Mutant (dlls/rpcrt4/rpc_assoc.c): SG_MUTANT_CTX_IN_EXCLUSIVE.
# rpcrt4 (6117): a client binds "<object>@ncalrpc:" with no endpoint, the server registered its
# dynamic ncalrpc endpoint with RpcEpRegister under that object UUID. NVDA's in-process helper
# talks to nvda.exe this way; RpcEpResolveBinding asked the endpoint mapper for the nil object
# only and found nothing, so the helper never registered with NVDA. widl-generated client and
# server in two processes (test/ctxshare-probe.c, test/ctxshare.idl).
#   WINE=/path/to/build/wine test/ctxshare-gate.sh   (WIDL=... when not beside $WINE)
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
T=$(mktemp -d /var/tmp/sg-ctxshare.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
SRV=""
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
cp "$HERE/ctxshare.idl" "$T/"
(cd "$T" && "$WIDL" --win64 -Oif --prefix-server=s_ -h -H ctxshare.h -o /dev/null ctxshare.idl \
    && "$WIDL" --win64 -Oif --prefix-server=s_ -c -o ctxshare_c.c ctxshare.idl \
    && "$WIDL" --win64 -Oif --prefix-server=s_ -s -o ctxshare_s.c ctxshare.idl) \
    || { echo "FAIL  widl did not run"; exit 1; }
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -I"$T" -o "$T/probe.exe" "$HERE/ctxshare-probe.c" \
    "$T/ctxshare_c.c" "$T/ctxshare_s.c" -lrpcrt4 || { echo "FAIL  probe did not build"; exit 1; }
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
printf '%s\n' "$out" | grep -qx 'SHARED 1' && echo "PASS  two [in] calls on one context handle run together (the second reaches the server while the first holds it)" || { echo "FAIL  [in] calls on one handle wait for each other: $(printf '%s' "$out" | grep -E 'SHARED|POKE_MS|EXCEPTION' | tr '\n' ' ')"; RC=1; }
printf '%s\n' "$out" | grep -qx 'EXCL 1' && echo "PASS  an [in, out] call has the handle alone (an [in] call waits for it)" || { echo "FAIL  [in, out] exclusivity: $(printf '%s' "$out" | grep -E 'EXCL|POKEX_MS' | tr '\n' ' ')"; RC=1; }
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
