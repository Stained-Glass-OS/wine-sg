#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The first fixes from the unimplemented-import sweep (wine-sg 7853; the table:
# /var/tmp/gaps2/outbox/import-sweep.md). Functions popular programs import that aborted them when
# called -- an "unimplemented function" import or a `@ stub` -- now answer as documented:
# NCryptCreateClaim (Chrome, Brave, Signal, Vivaldi), SetCredentialsAttributesA/W and
# ResolveIpNetEntry2 (msquic in Paint.NET and ShareX), I_RpcOpenClientProcess (Chrome's and
# Brave's elevation services), MFCreateTranscodeTopology and WlanGetNetworkBssList (Zoom), the
# Fwpm filter/provider/transaction/sublayer calls (Brave's VPN service; they add nothing: no
# filtering engine opens), InternetSetCookieEx2 (PotPlayer). test/sweepfix1-probe.c, 64/32-bit.
#   WINE=/opt/wine-sg/bin/wine test/sweepfix1-gate.sh
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
T=$(mktemp -d /var/tmp/sg-sweepfix1.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
# Wine's headers (mingw has no webservices.h): a build tree's, or the installed ones
W=$(dirname "$WINE")
for a in x86_64 i686; do
    built=
    for inc in "-I$W/include -I$W/../wine-10.0/include" "-I$W/../include/wine/windows" "-I/opt/wine-sg/include/wine/windows"; do
        TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 $inc -o "$T/probe-$a.exe" "$HERE/sweepfix1-probe.c" -lsecur32 -liphlpapi -lwininet -lwlanapi -lws2_32 2>/dev/null \
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
