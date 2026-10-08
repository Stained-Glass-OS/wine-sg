#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Small stubs (patches/sg/1608), under Xvfb: test/devfamily-probe.c.
# RtlGetDeviceFamilyInfoEnum gave version 0 (WinRT apps read the device
# family version); EtwEventSetInformation accepted anything;
# GetKeyboardLayout of another thread logged a FIXME on every call and
# answered for threads that do not exist.
#
#   WINE=/opt/wine-sg/bin/wine test/devfamily-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_DEVICE_FAMILY_VERSION (ntdll/rtl.c),
# SG_MUTANT_NO_ETW_INFO_CHECK (ntdll/misc.c),
# SG_MUTANT_NO_OTHER_THREAD_LAYOUT (win32u/input.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${DEVFAMILY_DPY:-367}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-devfamily.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/devfamily-probe.exe" "$HERE/devfamily-probe.c" \
    -lgdi32 -luser32 || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/devfamily-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && ! printf '%s\n' "$out" | grep -q '^FAIL' && exit 0
printf '%s\n' "$out" | grep -q '^RESULT:' || echo "RESULT: FAIL (the probe gave no result)"
exit 1
