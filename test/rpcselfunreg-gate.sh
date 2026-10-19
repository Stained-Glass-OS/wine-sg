#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An RPC call that unregisters its own interface returns (patches/sg/5938).
#
# COM unregisters an interface, waiting for its calls to complete, when the
# call that ends an object releases its last stub; the waiting call was one
# of them, so it waited for itself and held combase's interface list: the
# Click-to-Run service stopped answering and Office setup stayed at 1%.
#
#   WINE=/opt/wine-sg/bin/wine test/rpcselfunreg-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-rpcselfunreg.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/rpcselfunreg-probe.c" -lrpcrt4 || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
case "$out" in
*RETURNED*) echo "PASS  a call that unregisters its own interface (waiting for calls) returns"; echo "RESULT: PASS"; exit 0 ;;
*) echo "FAIL  the call waited for itself"; echo "RESULT: FAIL"; exit 1 ;;
esac
