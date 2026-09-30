#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A window stays responsive while its thread copies (patches/sg/0592). File
# Explorer's Paste of many files stopped drawing until the copy was done, and
# the progress window dragged over it left copies of itself (David). Now
# SHFileOperation copies on a thread of its own while the caller's thread
# handles its messages. The probe pings its window during a 1500-file copy.
#
#   WINE=/opt/wine-sg/bin/wine test/fileopresponsive-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fileopresp.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/fileopresponsive-probe.exe" "$HERE/fileopresponsive-probe.c" -lshell32 || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/fileopresponsive-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
timeout 300 "$WINE" 'C:\fileopresponsive-probe.exe' > "$T/out" 2>/dev/null; r=$?
echo "      $(tr -d '\r' < "$T/out")"
[ $r = 0 ] && { echo "PASS  the window answers during a long copy"; echo "RESULT: PASS"; exit 0; }
echo "FAIL  the window does not answer during the copy"; echo "RESULT: FAIL"; exit 1
