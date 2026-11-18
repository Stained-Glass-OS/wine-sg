#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# WWSAPI's named pipe binding (WS_NAMEDPIPE_CHANNEL_BINDING, wine-sg 7852; the N-WEBSERVICES
# transports row): it was "not implemented" (E_NOTIMPL from WsCreateChannel and
# WsCreateListener). A net.pipe service and its client in one program (test/wsnetpipe-probe.c,
# 64- and 32-bit): a duplex session both ways; the service published as WCF's
# NetNamedPipeBinding publishes one (the probe finds it by WCF's scheme, from a path below the
# service's); .NET message framing on the pipe (a raw preamble answered with PreambleAck); a
# second service at the same address refused; the name withdrawn on close; no service, no channel.
#   WINE=/opt/wine-sg/bin/wine test/wsnetpipe-gate.sh
#   (mutant SG_MUTANT_WS_PIPE_NOT_FRAMED, dlls/webservices/channel.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-wsnetpipe.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
# Wine's headers (mingw has no webservices.h): a build tree's, or the installed ones
W=$(dirname "$WINE")
for a in x86_64 i686; do
    built=
    for inc in "-I$W/include -I$W/../wine-10.0/include" "-I$W/../include/wine/windows" "-I/opt/wine-sg/include/wine/windows"; do
        TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 $inc -o "$T/probe-$a.exe" "$HERE/wsnetpipe-probe.c" -lwebservices 2>/dev/null \
            && { built=1; break; }
    done
    [ -n "$built" ] || { echo "FAIL  the probe ($a) did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
