#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winhttp gap-A batch (patch 3221), 64- and 32-bit: test/fastwinhttp-probe.c, with its own HTTP server on
# 127.0.0.1 (nothing leaves the machine): stored and inherited options, NULL-handle options, proxies, header
# names, 1xx answers, header/drain bounds, EXPIRE_CONNECTION, MAX_CONNS_PER_SERVER, decompression, request
# times/stats, extended error, code page, shared credentials, the proxy's CONNECT answer, connect retries,
# IWinHttpRequest::Option / SetClientCertificate / response stream.
#
#   WINE=/opt/wine-sg/bin/wine test/fastwinhttp-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_WH_* (winhttp: session.c request.c net.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-fastwinhttp.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/fastwinhttp-probe.c" \
        -lwinhttp -lws2_32 -lole32 -loleaut32 -luuid -lcrypt32 -ladvapi32 \
        || { echo "FAIL  probe did not build ($a)"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 300 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
    "$WINESERVER" -k 2>/dev/null
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
