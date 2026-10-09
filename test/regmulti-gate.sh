#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ntdll registry odds and ends (patches/sg/2205), 64- and 32-bit (through
# wow64): test/regmulti-probe.c checks NtQueryMultipleValueKey (was a stub that
# never filled the caller's buffers), the NtQueryKey Virtualization /
# HandleTags / Trust classes, NtSetInformationKey class and length validation,
# NtEnumerateValueKey's PartialInformationAlign64 and NtOpenKeyEx options.
#
#   WINE=/opt/wine-sg/bin/wine test/regmulti-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (ntdll/unix/registry.c): SG_MUTANT_MULTI_NOCOPY,
# SG_MUTANT_NO_FLAGCLASSES, SG_MUTANT_SETINFO_NOCHECK.
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
T=$(mktemp -d /var/tmp/sg-regmulti.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/regmulti-probe.c" \
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
