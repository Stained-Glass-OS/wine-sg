#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# File Explorer's copy progress window and the end of the copy (patches/sg/0980).
# In the regression walk (2026-10-05) a paste into the same folder, after
# "Keep both", left the progress window at "100% complete" for good, Cancel
# said "Cancelling..." for good and File Explorer hung: the copy ended just
# as the window was being made (half a second in), found no window yet to
# close, and waited for the window's thread forever.
#
# The probe copies a folder sized to take about a second, with the progress
# window, many times over: every copy must return and leave no window. With
# SG_FILEOP_SHOW_DELAY (the moment between deciding to show the window and
# showing it widened to 1.5 s) every round hits that moment; then rounds of
# the real timing, around the half second.
# Mutant: SG_MUTANT_FILEOP_LOST_CLOSE (dlls/shell32/fileopdlg.c) hangs.
#
#   WINE=/opt/wine-sg/bin/wine test/fileoprace-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fileoprace.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/fileoprace-probe.exe" "$HERE/fileoprace-probe.c" -lshell32 || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
DPY=":$(cat "$T/display")"
case "$DPY" in :|:0) echo "FAIL  no display of our own"; exit 1 ;; esac
export DISPLAY="$DPY"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/fileoprace-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c" || exit 1
RC=0
timeout -s KILL 400 env SG_FILEOP_SHOW_DELAY=1500 "$WINE" 'C:\fileoprace-probe.exe' 8 1000 > "$T/out1" 2>/dev/null; r1=$?
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
tr -d '\r' < "$T/out1" | sed 's/^/      /'
if [ $r1 = 0 ] && [ "$(grep -c '^ROUND' "$T/out1")" = 8 ]; then
    echo "PASS  a copy that ends as its progress window is being made: all 8 return, no window left"
else echo "FAIL  a copy ending as its progress window was made hung or left its window (exit $r1)"; RC=1; fi
timeout -s KILL 600 "$WINE" 'C:\fileoprace-probe.exe' 24 520 > "$T/out2" 2>/dev/null; r2=$?
tr -d '\r' < "$T/out2" | sed -n '1p;$p' | sed 's/^/      /'
if [ $r2 = 0 ] && [ "$(grep -c '^ROUND' "$T/out2")" = 24 ]; then
    echo "PASS  24 copies of about half a second (when the window comes): all return, no window left"
else echo "FAIL  copies of about half a second: one hung or left its window (exit $r2: $(grep -E 'HANG|LEFT' "$T/out2" | head -2 | tr '\r\n' '  '))"; RC=1; fi
if [ "$RC" = 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
