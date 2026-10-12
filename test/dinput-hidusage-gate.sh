#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Top level HID usage -> DirectInput device type table (patches/sg/2879):
# test/dinput-hidusage-probe.c compiles dlls/dinput/hid_usage_type.h (from a wine
# source tree, $WINE_SRC) and checks the type/subtype of every top level usage,
# running the probe under Wine. The dispatch in hid_joystick_device_try_open is
# not exercised: winebus' uinput/evdev devices always report a gamepad top level
# usage, and /dev/uhid and /dev/hidraw need root.
#
#   WINE=/opt/wine-sg/bin/wine WINE_SRC=/path/to/wine-10.0 test/dinput-hidusage-gate.sh
# Mutants (dinput, -DSG_MUTANT_x): UT_PAGES, UT_MULTIAXIS, UT_DRIVING, UT_GAME.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE_SRC="${WINE_SRC:-/var/tmp/drafter-5/build/wine-10.0}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f "$WINE_SRC/dlls/dinput/hid_usage_type.h" ] || { echo "SKIP: no dlls/dinput/hid_usage_type.h in $WINE_SRC"; exit 77; }
T=$(mktemp -d /var/tmp/sg-hidusage.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
# the mutant defines must reach the header: pass them through CFLAGS_EXTRA
TMPDIR=/var/tmp "$MINGW" -O1 ${SG_UT_DEFS:-} -I"$WINE_SRC/dlls/dinput" -o "$T/dinput-hidusage-probe.exe" \
    "$HERE/dinput-hidusage-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/dinput-hidusage-probe.exe" "$WINEPREFIX/drive_c/"
( cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 env DISPLAY= "$WINE" dinput-hidusage-probe.exe 2>/dev/null </dev/null | sed -u 's/\r$//' > "$T/probe.out" )
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
