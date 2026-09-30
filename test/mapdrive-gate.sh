#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Map Network Drive and Disconnect Network Drives (patches/sg/0594). The
# dialogs were stubs: File Explorer could not map a drive (David). Here the
# dialogs come up, offer drive letters, refuse a folder that is not a network
# folder, and cancel. Mapping a real share needs sg-netmountd: QA VM.
#
#   WINE=/opt/wine-sg/bin/wine test/mapdrive-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mapdrive.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/mapdrive-probe.exe" "$HERE/mapdrive-probe.c" -lmpr || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/mapdrive-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
timeout 120 "$WINE" 'C:\mapdrive-probe.exe' > "$T/out" 2>/dev/null; r=$?
sed 's/^/      /' "$T/out" | tr -d '\r'
[ $r = 0 ] && { echo "PASS  Map Network Drive and Disconnect Network Drives work as dialogs"; echo "RESULT: PASS"; exit 0; }
echo "FAIL  the network drive dialogs"; echo "RESULT: FAIL"; exit 1
