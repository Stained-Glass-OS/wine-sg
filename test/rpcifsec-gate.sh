#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# RPC interface security (wine-sg 7728): a security callback (RpcServerRegisterIf2) is called for each
# call and can refuse it -- Wine stored it and never called it, so every interface was open to every
# caller; RPC_IF_ALLOW_LOCAL_ONLY turns away ncacn_ip_tcp; RpcServerInqCallAttributes (a stub) answers
# inside a call; and a procedure number past the dispatch table ends with the fault instead of calling
# past the table. One process is server and client (ncalrpc, loopback TCP); test/rpcifsec-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/rpcifsec-gate.sh
# Mutant: SG_MUTANT_RPC_NO_IF_SECURITY (dlls/rpcrt4/rpc_server.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-rpcifsec.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
mkdir -p "$T/p64" "$T/p32"
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64/probe.exe" "$HERE/rpcifsec-probe.c" -lrpcrt4 &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32/probe.exe" "$HERE/rpcifsec-probe.c" -lrpcrt4 ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    out=$(cd "$T/$p" && timeout -s KILL 120 "$WINE" "$T/$p/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
        echo "ok   $p ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
    else
        echo "FAIL $p"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
    fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
