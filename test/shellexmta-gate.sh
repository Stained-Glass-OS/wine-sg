#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ShellExecute of a shortcut from a multithreaded-apartment thread (patches/sg/8413), 64- and 32-bit:
# test/shellexmta-probe.c makes a .lnk to itself and opens it from an MTA thread and (as before)
# from an STA thread; the shortcut's target must start. SG Store (RO_INIT_MULTITHREADED) opened
# nothing for apps whose Start entry is a shortcut in a folder (OpenRA).
#   WINE=/opt/wine-sg/bin/wine test/shellexmta-gate.sh
# Mutant: SG_MUTANT_SHELLEX_NO_MTA (dlls/shell32/shlexec.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-shellexmta.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    mkdir -p "$T/$a"
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -municode -o "$T/$a/probe.exe" "$HERE/shellexmta-probe.c" -lole32 -luuid -lshell32 ||
        { echo "FAIL  $a probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T/$a" && timeout -s KILL 120 "$WINE" "$T/$a/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
