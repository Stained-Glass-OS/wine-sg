#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A standard user may read a device's driver key (patches/sg/0580).
# setupapi opened the device-class root with KEY_CREATE_SUB_KEY even to read a
# driver key, and a standard user may not create under HKLM: every
# SetupDiOpenDevRegKey(DIREG_DRV) failed with ERROR_ACCESS_DENIED (Audacity 4's
# audio device enumeration, alex on the shared machine prefix). The probe
# locks the Class key read-only for everyone -- HKLM as a standard user has it
# -- and opens a present device's driver key read-only.
#
#   WINE=/opt/wine-sg/bin/wine test/drvkey-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-drvkey.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/drvkey-probe.exe" "$HERE/drvkey-probe.c" -lsetupapi -ladvapi32 || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 800x600x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/drvkey-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" 'C:\drvkey-probe.exe' 2>/dev/null | tr -d '\r')
printf "      %s\n" "$out"
case "$out" in
drvkey=ok*) echo "PASS  a driver key opens for reading where the class root is read-only"; echo "RESULT: PASS"; exit 0 ;;
drvkey=nodevice*) echo "SKIP: no device with a driver key in a fresh prefix"; exit 77 ;;
*) echo "FAIL  reading a driver key needed the right to create under HKLM ($out)"; echo "RESULT: FAIL"; exit 1 ;;
esac
