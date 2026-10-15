#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE rpcrt4: NdrConvert2 (big-endian integers, EBCDIC characters, IBM/VAX/Cray floats), widl-generated stubs in two
# processes over ncalrpc and ncacn_ip_tcp on 127.0.0.1, 64+32 bit; the requests and responses are built by hand.
#
#   WINE=/path/to/build/wine test/rpcrt4-convert-gate.sh   (WINESERVER=..., WIDL=... when not beside $WINE)
# Mutants: CONVERT_* (see patches/notes/3312.txt).  SG_QUICK=1: 64-bit only.
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
T=$(mktemp -d /var/tmp/sg-rpcrt4-conv.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
SRV=""
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    case $a in x86_64) m=--win64; ;; *) m=--win32; esac
    mkdir -p "$T/$a"
    cp "$HERE/rpcrt4-convert.idl" "$T/$a/"
    (cd "$T/$a" && "$WIDL" $m -Oif --prefix-server=s_ -h -H rpcrt4-convert.h -o /dev/null rpcrt4-convert.idl \
        && "$WIDL" $m -Oif --prefix-server=s_ -c -o rpcrt4-convert_c.c rpcrt4-convert.idl \
        && "$WIDL" $m -Oif --prefix-server=s_ -s -o rpcrt4-convert_s.c rpcrt4-convert.idl) \
        || { echo "FAIL  widl did not run ($a)"; exit 1; }
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -D"small=signed char" -I"$T/$a" -o "$T/probe-$a.exe" "$HERE/rpcrt4-convert-probe.c" \
        "$T/$a/rpcrt4-convert_c.c" "$T/$a/rpcrt4-convert_s.c" -lrpcrt4 -lws2_32 \
        || { echo "FAIL  probe did not build ($a)"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
ARCHES="x86_64 i686"; [ "${SG_QUICK:-}" = 1 ] && ARCHES=x86_64
for a in $ARCHES; do
    echo "== $a: direct calls of NdrConvert and NdrConvert2"
    out=$(cd "$T" && timeout -s KILL 100 env DISPLAY= "$WINE" "$T/probe-$a.exe" direct x 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
    for t in "ncalrpc sg_conv_ep" "ncacn_ip_tcp 49188"; do
        for pair in "server becall" "server rawtcp" "outserver outclient" "outserver outraw"; do
            set -- $pair
            case "$2:$t" in rawtcp:ncalrpc*|outraw:ncalrpc*) continue;; esac
            echo "== $a $t: $1 / $2"
            : > "$T/server.out"
            (cd "$T" && exec timeout -s KILL 240 env DISPLAY= "$WINE" "$T/probe-$a.exe" $1 $t >"$T/server.out" 2>&1 </dev/null) &
            SRV=$!
            n=0
            until grep -q READY "$T/server.out" 2>/dev/null || [ $n -ge 60 ]; do sleep 1; n=$((n + 1)); done
            grep -q READY "$T/server.out" || { echo "FAIL  the server did not start"; cat "$T/server.out"; RC=1; kill $SRV 2>/dev/null; SRV=""; continue; }
            case "$2" in rawtcp|outraw) arg=${t#* };; *) arg=$t;; esac
            out=$(cd "$T" && timeout -s KILL 200 env DISPLAY= "$WINE" "$T/probe-$a.exe" $2 $arg 2>/dev/null </dev/null | tr -d '\r')
            printf '%s\n' "$out" | grep -v '^PASS' | sed 's/^/      /'
            printf '%s\n' "$out" | grep -c '^PASS' | sed 's/^/      passed: /'
            printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || { RC=1; grep '^server:' "$T/server.out" | head -20 | sed 's/^/      /'; }
            n=0
            while kill -0 $SRV 2>/dev/null && [ $n -lt 10 ]; do sleep 1; n=$((n + 1)); done
            kill $SRV 2>/dev/null; SRV=""
            "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
        done
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
