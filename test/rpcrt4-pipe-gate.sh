#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE rpcrt4: NDR [pipe] (widl-generated client and server in two processes, ncalrpc and ncacn_ip_tcp on 127.0.0.1), 64+32 bit.
#
#   WINE=/path/to/build/wine test/rpcrt4-pipe-gate.sh   (WINESERVER=..., WIDL=... when not beside $WINE)
# Mutants: PIPE_* (see patches/notes/3309.txt).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
WIDL="${WIDL:-$(dirname "$WINE")/tools/widl/widl}"
[ -x "$WIDL" ] || WIDL="$(dirname "$WINE")/widl"
[ -x "$WIDL" ] || { echo "SKIP: no widl"; exit 77; }
"$WIDL" --help 2>&1 | grep -q . || exit 77
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-rpcrt4-pipe.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
SRV=""
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    case $a in x86_64) m=--win64; ;; *) m=--win32; esac
    mkdir -p "$T/$a"
    cp "$HERE/rpcrt4-pipe.idl" "$T/$a/"
    (cd "$T/$a" && "$WIDL" $m -Oif --prefix-server=s_ -h -H rpcrt4-pipe.h -o /dev/null rpcrt4-pipe.idl \
        && "$WIDL" $m -Oif --prefix-server=s_ -c -o rpcrt4-pipe_c.c rpcrt4-pipe.idl \
        && "$WIDL" $m -Oif --prefix-server=s_ -s -o rpcrt4-pipe_s.c rpcrt4-pipe.idl) \
        || { echo "FAIL  widl did not run ($a)"; exit 1; }
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -I"$T/$a" -o "$T/probe-$a.exe" "$HERE/rpcrt4-pipe-probe.c" \
        "$T/$a/rpcrt4-pipe_c.c" "$T/$a/rpcrt4-pipe_s.c" -lrpcrt4 \
        || { echo "FAIL  probe did not build ($a)"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# SG_QUICK=1: 64-bit ncalrpc only; SG_QUICK=tcp: 64-bit ncacn_ip_tcp only (for the mutants)
ARCHES="x86_64 i686"; T1="ncalrpc sg_pipe_ep"; T2="ncacn_ip_tcp 49187"
case "${SG_QUICK:-}" in 1) ARCHES=x86_64; T2="";; tcp) ARCHES=x86_64; T1="";; esac
for a in $ARCHES; do
    for t in "$T1" "$T2"; do
        [ -n "$t" ] || continue
        for pair in "server client" "rawserver rawcheck" "server rawclient"; do
            set -- $pair
            echo "== $a $t: $1 / $2"
            : > "$T/server.out"
            (cd "$T" && exec timeout -s KILL 240 env DISPLAY= "$WINE" "$T/probe-$a.exe" $1 $t >"$T/server.out" 2>&1 </dev/null) &
            SRV=$!
            n=0
            until grep -q READY "$T/server.out" 2>/dev/null || [ $n -ge 60 ]; do sleep 1; n=$((n + 1)); done
            grep -q READY "$T/server.out" || { echo "FAIL  the server did not start"; cat "$T/server.out"; RC=1; kill $SRV 2>/dev/null; SRV=""; continue; }
            out=$(cd "$T" && timeout -s KILL 200 env DISPLAY= "$WINE" "$T/probe-$a.exe" $2 $t 2>/dev/null </dev/null | tr -d '\r')
            printf '%s\n' "$out" | sed 's/^/      /'
            printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || { RC=1; grep '^raw:' "$T/server.out" | sed 's/^/      server /'; }
            # the server leaves when the client called Stop
            n=0
            while kill -0 $SRV 2>/dev/null && [ $n -lt 15 ]; do sleep 1; n=$((n + 1)); done
            kill $SRV 2>/dev/null; SRV=""
            "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
        done
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
