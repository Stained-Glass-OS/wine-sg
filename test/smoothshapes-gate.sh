#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Our shell parts' round and slanted shapes are drawn smooth (wine-sg 1240,
# include/wine/sg_smooth.h): GDI draws rounded rectangles and slanted lines
# without antialiasing, so the Rounded taskbar's search box and plates,
# Notepad's and File Explorer's tabs and their close marks, and the flat
# drop-down lists' chevrons had stepped edges (David 2026-10-06: "review all
# the icons ... better resolution"). Under Xvfb, at 100%, distinct colours
# counted round each edge (jagged: the shape's colour and what is under it;
# smooth: the blends between):
#   1. the Rounded taskbar's search box, its round end: at least 8
#   2. Notepad's active tab, its rounded corner: at least 3 (white on #F3F3F3)
#   3. File Explorer's tab close mark: at least 6
#   4. a drop-down list's chevron (user32's flat combo box): at least 6
#
#   WINE=/opt/wine-sg/bin/wine test/smoothshapes-gate.sh
#   (mutant SG_MUTANT_JAGGED at the top of explorer/systray.c, notepad/main.c,
#   explorer/fileexplorer.c, user32/combo.c fails 1, 2, 3, 4)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb import convert "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-smoothshapes.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -municode -mwindows -o "$T/combo.exe" "$HERE/smoothshapes-combo.c" || { fail "the combo box probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Stained Glass\Style' /v Rounded /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Search' /v SearchboxTaskbarMode /t REG_DWORD /d 2 /f >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$T/run"; export XDG_RUNTIME_DIR="$T/run" SG_LOCK_CONTROL=/nonexistent
colours() { convert "$T/$1.png" -crop "$2" +repage -format %k info:; }
check() {  # check SHOT CROP MIN WHAT
    n=$(colours "$1" "$2")
    [ "${n:-0}" -ge "$3" ] 2>/dev/null && pass "$4 is smooth ($n colours)" || fail "$4 is jagged: ${n:-?} colours (want $3 or more)"
}

"$WINE" explorer /desktop=shell,1024x768 >/dev/null 2>&1 & sleep 8
import -window root "$T/shell.png"
check shell 9x40+59+724 8 "the Rounded taskbar's search box"
"$WINE" "$T/combo.exe" >/dev/null 2>&1 & sleep 3
import -window root "$T/combo.png"
check combo 16x12+306+157 6 "a drop-down list's chevron"
"$WINE" explorer 'C:\windows' >/dev/null 2>&1 & sleep 5
import -window root "$T/fe.png"
check fe 14x14+215+45 6 "File Explorer's tab close mark"
"$WINE" notepad >/dev/null 2>&1 & sleep 4
import -window root "$T/np.png"
check np 8x8+8+54 3 "Notepad's tab corner"
[ -n "${SG_SHOTS:-}" ] && cp "$T"/*.png "$SG_SHOTS"/ 2>/dev/null
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
