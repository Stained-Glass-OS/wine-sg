#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Files, heaps and device keys (patches/sg/1691), 64- and 32-bit:
# test/fileflags-probe.c finds files with FIND_FIRST_EX_CASE_SENSITIVE and
# FIND_FIRST_EX_ON_DISK_ENTRIES_ONLY, asks FileRemoteProtocolInfo of a local
# file, lists and walks its heaps through a TH32CS_SNAPHEAPLIST snapshot,
# and keeps a device's value in its hardware-profile-specific key
# (DICS_FLAG_CONFIGSPECIFIC). These were FIXMEs and stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/wscatalog-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_CASE_INSENSITIVE, SG_MUTANT_REMOTE_FOR_ALL (kernelbase/file.c),
# SG_MUTANT_NO_HEAP_WALK (kernel32/toolhelp.c), SG_MUTANT_NO_CONFIG_SPECIFIC
# (setupapi/devinst.c).
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
T=$(mktemp -d /var/tmp/sg-fileflags.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/fileflags-probe.c" -lsetupapi \
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
