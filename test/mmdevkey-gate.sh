#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A standard user's audio endpoints have properties (patches/sg/0581).
# mmdevapi kept its device registry only in HKLM and opened it for writing; a
# standard user may not write there, so on the shared machine prefix every
# endpoint had no property store and the device list changed between
# enumerations -- Audacity 4 overran its device table and crashed. The probe
# makes the key read-only (as HKLM is to a standard user) and, in a new
# process, every active endpoint must still have a name. Security
# descriptors are not saved with the registry: one server holds both steps.
# Needs the machine's own sound server (Pulse/PipeWire): no endpoints, SKIP.
#
#   WINE=/opt/wine-sg/bin/wine test/mmdevkey-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mmdevkey.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/mmdevkey-probe.exe" "$HERE/mmdevkey-probe.c" -lole32 -luuid -ladvapi32 -lpropsys \
    || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/mmdevkey-probe.exe" "$WINEPREFIX/drive_c/"
"$WINESERVER" -p
cd "$WINEPREFIX/drive_c" || exit 1
lock=$(timeout 60 "$WINE" 'C:\mmdevkey-probe.exe' lock 2>/dev/null | tr -d '\r')
out=$(timeout 60 "$WINE" 'C:\mmdevkey-probe.exe' check 2>/dev/null | tr -d '\r')
printf '      %s; %s\n' "$lock" "$out"
case "$out" in
mmdev=ok*) echo "PASS  every audio endpoint has its properties where HKLM is read-only"; echo "RESULT: PASS"; exit 0 ;;
mmdev=nodevices*) echo "SKIP: no audio endpoints (no sound server)"; exit 77 ;;
*) echo "FAIL  endpoints without properties when HKLM is read-only ($out)"; echo "RESULT: FAIL"; exit 1 ;;
esac
