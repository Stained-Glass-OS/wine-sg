#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Open with (patches/sg/0594). SHOpenWithDialog and OpenAs_RunDLL were stubs
# and the shell's menus had no "Open with" (David: "a right click Open with
# option in the file manager is important"). A file's menu has it; the dialog
# lists the app that opens the type now and the others registered for it; a
# choice with "Always use this app" becomes the user's (UserChoice).
#
#   WINE=/opt/wine-sg/bin/wine test/openwith-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-openwith.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/openwith-probe.exe" "$HERE/openwith-probe.c" -lshell32 -lole32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/openwith-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
timeout 120 "$WINE" 'C:\openwith-probe.exe' > "$T/out" 2>/dev/null; r=$?
sed 's/^/      /' "$T/out" | tr -d '\r'
[ $r = 0 ] && { echo "PASS  Open with: in the menu, the apps listed, the choice kept"; echo "RESULT: PASS"; exit 0; }
echo "FAIL  Open with"; echo "RESULT: FAIL"; exit 1
