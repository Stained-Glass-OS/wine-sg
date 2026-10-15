#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# BITS upload and upload-reply jobs (patch 3577), 64- and 32-bit: test/fastb-bitsup-probe.c
# against test/fastb-bitsup-server.py (a local server speaking the BITS upload protocol).
# Uploads were never sent; the reply calls were stubs; SetPriority and SetNotifyFlags
# answered E_NOTIMPL for bad values.
#
#   WINE=/opt/wine-sg/bin/wine test/fastb-bitsup-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc python3; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-bitsup.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
SP=
cleanup() { [ -n "$SP" ] && kill "$SP" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/fastb-bitsup-probe.c" -lole32 -luuid &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/fastb-bitsup-probe.c" -lole32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
python3 "$HERE/fastb-bitsup-server.py" > "$T/port" & SP=$!
i=0; while [ ! -s "$T/port" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
PORT=$(head -1 "$T/port")
[ -n "$PORT" ] || { echo "FAIL  the server did not start"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    echo "== $p"
    out=$(cd "$T" && timeout -s KILL 240 "$WINE" "$T/$p.exe" "$PORT" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
