#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# win32u/wow64win: D3DKMTQueryStatistics (8004) -- the adapter's segments (its Vulkan memory heaps:
# size, this process's usage, aperture), node and VidPN source counts, invalid queries refused; it was a
# stub returning success with nothing filled in. 64-bit and 32-bit (the WoW64 thunk: hProcess shifts the
# 32-bit layout). Vulkan on the gate's Xvfb is lavapipe.
#   WINE=/opt/wine-sg/bin/wine test/kmtstats-gate.sh
# Mutant: SG_MUTANT_KMT_STATS_STUB (win32u/d3dkmt.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kmtstats.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/kmtstats-probe.c" -lgdi32 -luser32 &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/kmtstats-probe.c" -lgdi32 -luser32 ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    for dll in win32u; do
        if command -v xvfb-run >/dev/null; then X="xvfb-run -n 281 -s -screen\ 0\ 1024x768x24"; else X=""; fi
        out=$(cd "$T" && eval timeout -s KILL 120 $X '"$WINE"' '"$T/$p.exe"' $dll 2>/dev/null </dev/null | tr -d '\r')
        if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
            echo "ok   $p $dll ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
        else
            echo "FAIL $p $dll"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
        fi
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
