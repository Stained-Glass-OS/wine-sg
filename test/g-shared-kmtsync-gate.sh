#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# G-SHARED: D3DKMT synchronization objects (win32u, patch 3323): Create/Open/Destroy of mutex, semaphore, fence, CPU
# notification and monitored fence objects, SignalSynchronizationObjectFromCpu and WaitForSynchronizationObjectFromCpu
# (fence values, rewinds, semaphore limits, wait any/all, events), and a fence shared with another process by a global
# handle, an inherited NT handle, a named NT handle, and together with a resource: the child waits for the parent's values
# 1, 2, 3 in order and answers 10.
# Probe: test/g-shared-kmtsync-probe.c, 64-bit and 32-bit, under Xvfb.
#
#   WINE=/opt/wine-sg/bin/wine test/g-shared-kmtsync-gate.sh
#   WINE_INCLUDE=<wine source include dir> when the installed headers lack ddk/d3dkmthk.h
# Mutants (win32u d3dkmt.c): SG_MUTANT_KMT_SYNC_REWIND, SG_MUTANT_KMT_SYNC_NOLIMIT, SG_MUTANT_KMT_SYNC_ANYALL,
# SG_MUTANT_KMT_SYNC_NOMERGE.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
DPY="${GSHARED_DPY:-253}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in Xvfb xdpyinfo x86_64-w64-mingw32-gcc; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-gsharedsy.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

inc_list="-idirafter ${WINE_INCLUDE:-/nonexistent} -idirafter $(dirname "$WINE")/include/wine/windows -idirafter /opt/wine-sg/include/wine/windows"
archs="x86_64-w64-mingw32-gcc"
command -v i686-w64-mingw32-gcc >/dev/null && archs="$archs i686-w64-mingw32-gcc"
n=0
for cc in $archs; do
    n=$((n + 1))
    TMPDIR=/var/tmp "$cc" -O2 -mwindows $inc_list -o "$T/probe$n.exe" "$HERE/g-shared-kmtsync-probe.c" 2>"$T/cc.err" \
        || { cat "$T/cc.err"; echo "FAIL  the probe did not build ($cc)"; exit 1; }
done

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

rc=0
i=1
while [ $i -le $n ]; do
    # a crashed probe leaves wine helpers holding a pipe: go through a file
    timeout -s KILL 90 "$WINE" "$T/probe$i.exe" >"$T/out.raw" 2>/dev/null </dev/null
    tr -d '\r' <"$T/out.raw" >"$T/out"
    echo "== probe $i ($([ $i = 1 ] && echo 64-bit || echo 32-bit))"
    cat "$T/out"
    if ! { grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out"; }; then
        grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
        rc=1
    fi
    i=$((i + 1))
done
[ $rc = 0 ] && echo "ALL PASS"
exit $rc
