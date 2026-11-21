#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CHOICE /T with no one at the console (patches/sg/8414): test/choiceto-probe.c starts
# "cmd /C choice /C Y /N /D Y /T 2 & echo done > marker" without a window, in a new console, and with
# stdin from NUL; each must end with the default answer after the timeout. Squirrel uninstallers
# (Mudlet, Trilium) delete themselves that way and their cmd stayed forever.
#   WINE=/opt/wine-sg/bin/wine test/choiceto-gate.sh
# Mutant: SG_MUTANT_CHOICE_CONSOLE_NO_TIMEOUT (programs/cmd/builtins.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-choiceto.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/choiceto-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
