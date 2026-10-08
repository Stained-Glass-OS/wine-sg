#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ReplaceFileW's merge, SystemPowerInformation, power capabilities and
# other small stubs (patches/sg/1616), 64- and 32-bit:
# test/replacefile-probe.c. ReplaceFileW logged "Ignoring flags" and gave
# the replaced file the new file's creation time and attributes; 32-bit
# programs got nothing for CallNtPowerInformation(SystemPowerInformation);
# GetPwrCapabilities was an XP desktop's canned answer.
#
#   WINE=/opt/wine-sg/bin/wine test/replacefile-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_REPLACE_MERGE (kernelbase/file.c),
# SG_MUTANT_NO_POWER_CAPS (ntdll/unix/system.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-replacefile.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/replacefile-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/replacefile-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    echo "== $p"
    SLEEPS=""
    grep -qw mem /sys/power/state 2>/dev/null && SLEEPS="${SLEEPS}s3"
    grep -qw disk /sys/power/state 2>/dev/null && SLEEPS="${SLEEPS}s4"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" "x$SLEEPS" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
