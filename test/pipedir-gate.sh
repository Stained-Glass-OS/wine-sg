#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The named pipe root directory lists the pipes (wine-sg 7744): FindFirstFile("\\.\pipe\*") and
# NtQueryDirectoryFile on \\.\pipe\ give the pipe names (EndOfFile = instances, AllocationSize = the
# maximum), with the first query's mask kept, restart and no-match errors as on Windows. Wine failed
# with ERROR_BAD_DEV_TYPE, and Visual Studio's ServiceHub client, which looks for its controller's
# pipe that way, never found it. test/pipedir-probe.c, 64- and 32-bit.
#   WINE=/opt/wine-sg/bin/wine test/pipedir-gate.sh
# Mutants: SG_MUTANT_NPFS_DIR_EMPTY (server/named_pipe.c), SG_MUTANT_NPFS_DIR_NO_MASK (dlls/ntdll/unix/file.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pipedir.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
mkdir -p "$T/p64" "$T/p32"
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64/probe.exe" "$HERE/pipedir-probe.c" -lntdll &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32/probe.exe" "$HERE/pipedir-probe.c" -lntdll ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    out=$(cd "$T/$p" && timeout -s KILL 120 "$WINE" "$T/$p/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
        echo "ok   $p ($(printf '%s\n' "$out" | grep -c '^ok') checks)"; printf '%s\n' "$out" | grep '^skip\|^info' 
    else
        echo "FAIL $p"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
    fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
