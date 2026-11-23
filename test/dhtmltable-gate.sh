#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The DHTML Editing Control's table commands and parameter objects (wine-sg 9607, dlls/dhtmled.ocx
# tables.c): DECMD_INSERTTABLE with DEInsertTableParam, row/column/cell commands, DEGetBlockFmtNamesParam.
#   WINE=/opt/wine-sg/bin/wine test/dhtmltable-gate.sh
# Mutant: SG_MUTANT_DH_NO_TABLES (dlls/dhtmled.ocx/tables.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${DHTMLTABLE_DPY:-338}"
[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
for need in Xvfb xdpyinfo; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dhtmltable.XXXXXX)
XP=""
export WINEPREFIX="$T/prefix" WINEDEBUG="${DHTMLTABLE_DEBUG:--all}" WINEDLLOVERRIDES="mscoree=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/dhtmltable-probe.c" -lole32 -loleaut32 -luuid -luser32 2>/dev/null \
    || { echo "FAIL  probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
i=0; while ! DISPLAY=":$DPY" xdpyinfo >/dev/null 2>&1 && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
export DISPLAY=":$DPY"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 180 "$WINE" probe.exe >"$T/probe.out" 2>"$T/probe.err" </dev/null)
tr -d '\r' < "$T/probe.out"
[ -n "${DHTMLTABLE_DEBUG:-}" ] && tail -n 200 "$T/probe.err"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
echo "RESULT: FAIL"
exit 1
