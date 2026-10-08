#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Preferred UI languages are kept and used (patches/sg/1644), 64- and 32-bit:
# test/uilangs-probe.c sets the process's and thread's preferred UI
# languages, reads them back with every merge flag, and loads a string in
# three languages (test/uilangs-probe.rc) as the lists change; then the
# user's list from the Linux LANGUAGE list. Set* kept nothing and every Get*
# returned the UI language before; GetThreadUILanguage and
# SetThreadUILanguage were stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/uilangs-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_THREAD_LANGS_IGNORED (ntdll/locale.c),
# SG_MUTANT_RESOURCE_LANGS_IGNORED (ntdll/resource.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc x86_64-w64-mingw32-windres i686-w64-mingw32-windres; do
    command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-uilangs.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export LANG=en_US.UTF-8 LC_ALL=
unset LANGUAGE
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
(cd "$T" && TMPDIR=/var/tmp x86_64-w64-mingw32-windres "$HERE/uilangs-probe.rc" -O coff -o r64.o &&
    TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o p64.exe "$HERE/uilangs-probe.c" r64.o &&
    TMPDIR=/var/tmp i686-w64-mingw32-windres "$HERE/uilangs-probe.rc" -O coff -o r32.o &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o p32.exe "$HERE/uilangs-probe.c" r32.o) || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    echo "== $p"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
    echo "== $p, LANGUAGE=de_DE:fr:en"
    out=$(cd "$T" && LANGUAGE=de_DE:fr:en timeout -s KILL 120 "$WINE" "$T/$p.exe" user en-US,de-DE,fr-FR 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
