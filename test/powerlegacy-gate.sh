#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The legacy numbered power scheme API (patches/sg/2405), 64- and 32-bit:
# test/powerlegacy-probe.c enumerates, reads, writes, activates and deletes
# power schemes by number, reads and writes the global and processor
# policies, and checks the legacy active scheme follows the GUID one. All
# eleven functions (EnumPwrSchemes, ReadPwrScheme, WritePwrScheme,
# DeletePwrScheme, Get/SetActivePwrScheme, Read/WriteGlobalPwrPolicy,
# Read/WriteProcessorPwrScheme, GetCurrentPowerPolicies) failed with
# ERROR_CALL_NOT_IMPLEMENTED.
#
#   WINE=/opt/wine-sg/bin/wine test/powerlegacy-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (powrprof/legacy.c): SG_MUTANT_LPW_NEWID, SG_MUTANT_LPW_ENUM_STOP,
# SG_MUTANT_LPW_NAMESIZE, SG_MUTANT_LPW_DELETE_ACTIVE, SG_MUTANT_LPW_ACTIVE_STALE.
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
T=$(mktemp -d /var/tmp/sg-powerlegacy.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/powerlegacy-probe.c" -lpowrprof -luser32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
for a in x86_64 i686; do
    echo "== $a"
    # each run starts from a fresh profile: the probe changes the schemes
    "$WINESERVER" -k 2>/dev/null; rm -rf "$WINEPREFIX"; mkdir -p "$WINEPREFIX"
    timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
    "$WINESERVER" -w
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
