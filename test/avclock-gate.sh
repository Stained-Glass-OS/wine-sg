#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The DirectSound renderer's clock follows the sound's play position (as on
# Windows), so video follows audio even when the sound device's clock drifts
# from the system's or the device holds a long, changing backlog (Bluetooth).
# A stand-in dsound.dll beside the probe plays 1% fast, then 1% slow: the
# graph's clock and stream position must follow what it played. With a sound
# device (PULSE_SERVER set to a server) the real dsound play cursor must be
# what the device has played, never the mix position ahead of it.
#
#   WINE=/opt/wine-sg/bin/wine test/avclock-gate.sh
# Mutants: SG_MUTANT_CLOCK_IS_SYSTEM (quartz/dsoundrender.c),
#          SG_MUTANT_DS_PLAY_IS_MIXPOS (dsound/buffer.c; needs a sound device).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-avclock.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
mkdir -p "$T/64" "$T/32" "$WINEPREFIX"
for a in 64:x86_64 32:i686; do
    d=${a%%:*}; cc=${a#*:}-w64-mingw32-gcc
    TMPDIR=/var/tmp $cc -O1 -o "$T/$d/p.exe" "$HERE/avclock-probe.c" -lole32 -lstrmiids -luuid -ldsound -ldxguid &&
        TMPDIR=/var/tmp $cc -O1 -shared -static-libgcc -o "$T/$d/dsound.dll" "$HERE/avclock-mockds.c" -Wl,--kill-at -ldxguid -luuid &&
        cp "$T/$d/p.exe" "$T/p$d.exe" || { echo "FAIL  probe did not build"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
run() { # NAME DIR ARGS... ; prints ok/FAIL; a SKIP result is not a failure
    name=$1; dir=$2; shift 2
    out=$(cd "$dir" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r')
    res=$(printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1)
    printf '%s\n' "$out" | grep '^info' | sed "s/^/     $name /"
    case "$res" in
    "RESULT: PASS") echo "ok   $name ($(printf '%s\n' "$out" | grep -c '^ok') checks)" ;;
    "RESULT: SKIP") echo "skip $name ($(printf '%s\n' "$out" | grep '^SKIP' | head -n1))" ;;
    *) echo "FAIL $name"; printf '%s\n' "$out" | grep -v '^ok\|^info'; RC=1 ;;
    esac
}
for d in 64 32; do
    for pm in 1010 990; do
        WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;dsound=n" run "p$d mock $pm" "$T/$d" "$T/$d/p.exe" mock $pm 8
    done
done
if [ -n "${PULSE_SERVER:-}" ]; then
    for d in 64 32; do run "p$d real" "$T" "$T/p$d.exe" real; done
else
    echo "skip real dsound play cursor (no PULSE_SERVER)"
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
