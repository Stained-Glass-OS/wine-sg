#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE: NtFlushProcessWriteBuffers / FlushProcessWriteBuffers runs the process-wide memory barrier (membarrier, or a page-protection change)
#
#   WINE=/path/to/build/wine test/ntdll-flushwrites-gate.sh   (WINESERVER=... when not beside $WINE)
# Mutants: listed in the patch's notes file.
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
T=$(mktemp -d /var/tmp/sg-ntdll-flushwrites.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+virtual WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/ntdll-flushwrites-probe.c"  \
        || { echo "FAIL  probe did not build ($a)"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>$T/err-$a.txt </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
    grep -q 'flushed (method [12], result 0)' "$T/err-$a.txt" && echo '      [wine trace seen: flushed (method [12], result 0)]' || { echo 'FAIL  wine trace missing: flushed (method [12], result 0)'; RC=1; }
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
