#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE: OleCreatePropertyFrameIndirect highlights dispidInitialProperty (IPerPropertyBrowsing page choice, IPropertyPage2::EditProperty)
#
#   WINE=/path/to/build/wine test/oleaut32-propframe-initial-gate.sh   (WINESERVER=... when not beside $WINE)
# Mutants: listed in the patch's notes file.
set -u
command -v Xvfb >/dev/null || { echo "SKIP: needs Xvfb"; exit 77; }
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-oleaut32-propframe-initial.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
XP=""
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY:-none}-lock"; rm -rf "$T" "${SG_GATE_HOME:-$T}"; }
trap cleanup EXIT INT TERM
# the sheet is a real window: a display of the gate's own (first free number from 276)
DPY=${DPY:-276}
while [ -e "/tmp/.X$DPY-lock" ]; do DPY=$((DPY+1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
sleep 1
export DISPLAY=":$DPY"
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/oleaut32-propframe-initial-probe.c" -lole32 -loleaut32 -luuid -lcomctl32 -luser32 \
        || { echo "FAIL  probe did not build ($a)"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 "$WINE" "$T/probe-$a.exe" 2>$T/err-$a.txt </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1

done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
