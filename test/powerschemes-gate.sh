#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Power schemes and the platform role (patches/sg/1699), 64- and 32-bit:
# test/powerschemes-probe.c reads, writes, duplicates and deletes power
# schemes and their settings, switches the active scheme and is told of it,
# and asks for the platform role. PowerGetActiveScheme, PowerReadDCValue,
# PowerEnumerate and PowerReadFriendlyName were not implemented,
# PowerSetActiveScheme and PowerWriteACValueIndex kept nothing, the platform
# role was always "desktop" and the notification registrations never called.
#
#   WINE=/opt/wine-sg/bin/wine test/powerschemes-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (powrprof/schemes.c): SG_MUTANT_ACTIVE_NOT_KEPT,
# SG_MUTANT_NO_INITIAL_VALUE, SG_MUTANT_ROLE_ANY_VERSION.
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
T=$(mktemp -d /var/tmp/sg-powerschemes.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/powerschemes-probe.c" -lpowrprof -luser32 \
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
