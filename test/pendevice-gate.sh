#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Devices.Input.PenDevice (patches/sg/9801): the class is registered (windows.ui.dll) and its
# statics answer GetFromPointerId as Windows does for a pointer that is no pen with a unique id -- S_OK
# and null (Chromium's pen id handler then gives the event no persistent device id). Five Electron
# programs logged "RoGetActivationFactory ... not registered" at start. 64- and 32-bit.
# test/pendevice-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/pendevice-gate.sh
# Mutant (dlls/windows.ui/main.c): SG_MUTANT_NO_PEN_FACTORY -> factory=0x80040154.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc missing"; exit 77; }
command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: i686-w64-mingw32-gcc missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pendevice.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in 64 32; do
    cc=x86_64-w64-mingw32-gcc; [ $a = 32 ] && cc=i686-w64-mingw32-gcc
    TMPDIR=/var/tmp $cc -O2 -o "$T/pen$a.exe" "$HERE/pendevice-probe.c" -lruntimeobject || { echo "FAIL  the $a-bit probe did not build"; exit 1; }
done
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 120 "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T"/pen*.exe "$WINEPREFIX/drive_c/"
RC=0
for a in 64 32; do
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" pen$a.exe 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s: %s\n' $a "$2"; else printf 'FAIL  %s: %s\n' $a "$2"; RC=1; fi; }
    chk '^factory=0$' "RoGetActivationFactory(Windows.Devices.Input.PenDevice) succeeds"
    chk '^statics=0$' "the factory has IPenDeviceStatics"
    chk '^agile=0$' "  and is agile"
    chk '^class=0 Windows.Devices.Input.PenDevice$' "  named Windows.Devices.Input.PenDevice"
    chk '^activate=0x80004001 1$' "PenDevice has no public constructor (E_NOTIMPL, null)"
    chk '^mouse=0 1$' "GetFromPointerId(the mouse): S_OK, null"
    chk '^unknown=0 1$' "GetFromPointerId(an unknown id): S_OK, null"
    chk '^zero=0 1$' "GetFromPointerId(0): S_OK, null"
    chk '^nullout=0x80004003$' "a null out pointer: E_POINTER"
    chk '^done=1$' "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
