#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# vcomp 64-bit dynamic loops (patches/sg/4523, fast mode), 64- and 32-bit:
# _vcomp_for_dynamic_init_i8/_next_i8 with chunked, guided and static
# schedules past 2^32, alone and in a team of four threads, every iteration
# once, and omp_get_wtick, in vcomp, vcomp90 .. vcomp140:
# test/vcomp-dynamic-i8-probe.c. All were spec stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/vcomp-dynamic-i8-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_DYNAMIC_I8_NO_ADVANCE (vcomp/main.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-vcomp-dynamic-i8.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/vcomp-dynamic-i8-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/vcomp-dynamic-i8-probe.c" ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    for dll in vcomp vcomp90 vcomp100 vcomp110 vcomp120 vcomp140; do
        out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" $dll 2>/dev/null </dev/null | tr -d '\r')
        if printf '%s\n' "$out" | grep -qx 'RESULT: PASS'; then
            echo "ok   $p $dll ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
        else
            echo "FAIL $p $dll"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
        fi
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
