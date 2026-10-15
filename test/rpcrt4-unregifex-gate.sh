#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE rpcrt4: RpcServerUnregisterIfEx waits for the calls and runs down (or forgets) the interface's context
# handles.  A real in-process ncalrpc client and server, stubs from widl (test/rpcrt4-unregifex.idl), 64+32 bit.
#
#   WINE=/path/to/build/wine test/rpcrt4-unregifex-gate.sh   (WINESERVER=... when not beside $WINE)
# Mutants: UNREG_EX_NOP, RUNDOWN_FLAG (see patches/notes/5115.txt).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
WIDL="${WIDL:-$(dirname "$WINE")/tools/widl/widl}"
[ -x "$WIDL" ] || WIDL="$(dirname "$WINE")/widl"
[ -x "$WIDL" ] || { echo "SKIP: no widl"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-rpcrt4-unregifex.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    case $a in x86_64) m=--win64; ;; *) m=--win32; esac
    mkdir -p "$T/$a"
    cp "$HERE/rpcrt4-unregifex.idl" "$T/$a/"
    (cd "$T/$a" && "$WIDL" $m -Oif --prefix-server=s_ -h -H rpcrt4-unregifex.h -o /dev/null rpcrt4-unregifex.idl \
        && "$WIDL" $m -Oif --prefix-server=s_ -c -o rpcrt4-unregifex_c.c rpcrt4-unregifex.idl \
        && "$WIDL" $m -Oif --prefix-server=s_ -s -o rpcrt4-unregifex_s.c rpcrt4-unregifex.idl) \
        || { echo "FAIL  widl did not run ($a)"; exit 1; }
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -I"$T/$a" -o "$T/probe-$a.exe" "$HERE/rpcrt4-unregifex-probe.c" \
        "$T/$a/rpcrt4-unregifex_c.c" "$T/$a/rpcrt4-unregifex_s.c" -lrpcrt4 \
        || { echo "FAIL  probe did not build ($a)"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 120 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
