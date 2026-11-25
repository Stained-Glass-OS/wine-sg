#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The shell extension handler keys every Windows installation has (patches/sg/9861):
# HKCR\{*,AllFilesystemObjects,Directory,Directory\Background,Drive,Folder}\shellex\ContextMenuHandlers,
# PropertySheetHandlers, DragDropHandlers and Directory's CopyHookHandlers. A shell extension's
# DllRegisterServer opens them to add itself, it does not create them: Bulk Rename Utility 4.1's
# BRUhere64.dll found no HKCR\Directory\shellex\ContextMenuHandlers, regsvr32 failed (exit 5) and
# its installer rolled the whole installation back. Each key must open in a new prefix.
#   WINE=/opt/wine-sg/bin/wine test/shellexkeys-gate.sh
# No mutant: the keys are inf data (wine.inf.in is not preprocessed); the gate fails on 10.0-1157.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-shellexkeys.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/shellexkeys-probe.c" -ladvapi32 || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rc=0
out=$(timeout 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -qx END || { echo "FAIL  the probe did not finish"; rc=1; }
n=$(printf '%s\n' "$out" | grep -c '^KEY ok ')
printf '%s\n' "$out" | sed -n 's/^KEY missing /FAIL  missing HKCR\\/p'
printf '%s\n' "$out" | grep -q '^KEY missing ' && rc=1
[ "$n" = 15 ] && printf 'PASS  all 15 handler keys are there\n' || rc=1
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
