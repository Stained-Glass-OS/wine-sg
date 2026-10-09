#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Perflib provider API (patches/sg/2201): test/perflib-probe.c runs a
# provider/counterset/instance/counter-value lifecycle through kernelbase,
# checks that a single-instance counter set refuses a second instance, and
# that a provider's own MemAllocRoutine/MemFreeRoutine is used for instance
# blocks. PerfStartProviderEx, PerfSetCounterSetInfo, PerfCreateInstance and
# PerfSetCounterRefValue were "semi-stub" FIXMEs on every call even though
# the lifecycle already worked; the single-instance rule and the custom
# allocator were not honoured at all.
#
#   WINE=/opt/wine-sg/bin/wine test/perflib-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (kernelbase/main.c): SG_MUTANT_NO_SINGLE_INSTANCE,
# SG_MUTANT_NO_CUSTOM_ALLOC.
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
T=$(mktemp -d /var/tmp/sg-perflib.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/perflib-probe.c" -lkernel32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
