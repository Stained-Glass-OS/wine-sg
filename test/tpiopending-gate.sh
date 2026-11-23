#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An idle thread-pool worker with I/O in flight is not retired (patches/sg/9050).
#
# digiKam froze for good when a photo was opened (qa4, 2026-11-23): Qt 6's QProcess pipe reader issues
# its overlapped ReadFile from a thread-pool worker; Wine retired that worker after 5 s idle, its exit
# cancelled the read, Qt took exiftool's stderr for broken and never saw the answer. On Windows the pool
# keeps a worker while ThreadIsIoPending says its I/O is in flight. test/tpiopending-probe.c (64 and
# 32 bit): ThreadIsIoPending FALSE/TRUE, a pool read issued before 8 s idle still delivers the data
# written after them, and a plain thread's exit still cancels its read (the Windows rule kept).
# Mutant: ntdll built with -DSG_MUTANT_TP_RETIRE_WITH_IO fails the idle-worker row.
#
#   WINE=/opt/wine-sg/bin/wine test/tpiopending-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-tpiopending.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/tpiopending-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/tpiopending-probe.c" ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/$p /"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
