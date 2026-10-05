#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A standard user reads a device's own key (patches/sg/0872).
# setupapi opened HKLM\System\CurrentControlSet\Enum with KEY_ALL_ACCESS to
# reach a device's key, and a standard user may not create under HKLM: every
# device's key was an invalid handle, so each property read failed with
# ERROR_INVALID_HANDLE ("fixme:setupapi:get_device_property Unhandled error
# 0x6"), hardware IDs included -- DYMO Connect on David's Latitude. The probe
# locks the Enum key read-only for everyone and reads every present device's
# hardware IDs and description.
#
#   WINE=/opt/wine-sg/bin/wine test/devkeyread-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-devkeyread.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/devkeyread-probe.exe" "$HERE/devkeyread-probe.c" -lsetupapi -ladvapi32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 800x600x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/devkeyread-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" 'C:\devkeyread-probe.exe' 2>/dev/null | tr -d '\r')
printf "      %s\n" "$out"
case "$out" in
devkey=ok*) echo "PASS  every present device's properties read where the Enum key is read-only"; echo "RESULT: PASS"; exit 0 ;;
devkey=nodevice*) echo "SKIP: no present device in a fresh prefix"; exit 77 ;;
*) echo "FAIL  reading a device's key needed the right to create under HKLM ($out)"; echo "RESULT: FAIL"; exit 1 ;;
esac
