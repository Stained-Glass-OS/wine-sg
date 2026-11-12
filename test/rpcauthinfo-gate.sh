#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The rest of RPC's security calls (wine-sg 7742): the authorization service and quality of service set
# with RpcBindingSetAuthInfoEx come back from RpcBindingInqAuthInfoEx (Wine said "not implemented" and
# answered RPC_C_AUTHZ_NONE, and refused any QOS); RpcGetAuthorizationContextForClient and
# RpcFreeAuthorizationContext (stubs) give a server an Authz context of its caller; and
# I_RpcBindingInqSecurityContext (a stub) answers. One process is server and client over ncalrpc;
# test/rpcauthinfo-probe.c. The authorization service/QOS part needs an SSP that can be set up here
# (NTLM, Kerberos or Negotiate); without one it is skipped.
#   WINE=/opt/wine-sg/bin/wine test/rpcauthinfo-gate.sh
# Mutants (dlls/rpcrt4/rpc_binding.c): SG_MUTANT_RPC_AUTHZ_NOT_KEPT, SG_MUTANT_RPC_QOS_NOT_REPORTED,
# SG_MUTANT_RPC_NO_AUTHZ_CONTEXT.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-rpcauthinfo.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
mkdir -p "$T/p64" "$T/p32"
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64/probe.exe" "$HERE/rpcauthinfo-probe.c" -lrpcrt4 -lauthz -ladvapi32 &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32/probe.exe" "$HERE/rpcauthinfo-probe.c" -lrpcrt4 -lauthz -ladvapi32 ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    out=$(cd "$T/$p" && timeout -s KILL 120 "$WINE" "$T/$p/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
        echo "ok   $p ($(printf '%s\n' "$out" | grep -c '^ok') checks)"; printf '%s\n' "$out" | grep '^skip\|^info' 
    else
        echo "FAIL $p"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
    fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
