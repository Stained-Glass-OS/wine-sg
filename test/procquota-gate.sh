#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Working set limits, pooled usage and critical processes (patches/sg/1701),
# 64- and 32-bit (through wow64): test/procquota-probe.c sets and reads its
# working set limits and their hard-limit flags, reads ProcessQuotaLimits
# as QUOTA_LIMITS and QUOTA_LIMITS_EX, its pooled usage, and makes itself
# critical with and without SeDebugPrivilege. GetProcessWorkingSetSizeEx
# gave 32 MB, SetProcessWorkingSetSizeEx did nothing, setting the quota was
# STATUS_NOT_IMPLEMENTED and the other classes were not implemented.
#
#   WINE=/opt/wine-sg/bin/wine test/procquota-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (ntdll/unix/process.c): SG_MUTANT_QUOTA_NOT_KEPT,
# SG_MUTANT_NO_PRIV_CHECK.
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
T=$(mktemp -d /var/tmp/sg-procquota.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/procquota-probe.c" -ladvapi32 \
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
