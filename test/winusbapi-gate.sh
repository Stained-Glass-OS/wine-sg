#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The WinUSB API without a device (patches/sg/6519): the descriptor parsers, the error answers
# for bad handles and for a handle that is not a WinUSB device, the driver and its INF. The
# path to a real device is test-winusb's (a USB gadget on a VM: test/usbgadget-setup.sh).
#   WINE=/opt/wine-sg/bin/wine test/winusbapi-gate.sh
# Mutant: SG_MUTANT_WINUSB_PARSE_UNBOUNDED (winusb.dll main.c: a descriptor past the buffer).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-winusbapi.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/winusbapi-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 180 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check exports              "winusb.dll exports the API"
check parse_alternate      "ParseConfigurationDescriptor finds an alternate setting"
check parse_class          "and an interface by class"
check parse_none           "and nothing for an interface that is not there"
check parse_endpoint       "ParseDescriptors finds the first endpoint"
check parse_next           "and the next one from where it was"
check parse_class_specific "and a class-specific descriptor"
check parse_truncated      "and not one past the buffer's end"
check init_invalid         "WinUsb_Initialize of an invalid handle: ERROR_INVALID_HANDLE"
check free_null            "WinUsb_Free(NULL): ERROR_INVALID_HANDLE"
check query_null           "WinUsb_QueryPipe(NULL): ERROR_INVALID_HANDLE"
check read_bogus           "a transfer on memory that is not a handle: ERROR_INVALID_HANDLE"
check init_file            "a file is not a WinUSB device"
check driver               "winusb.sys is installed"
check inf                  "winusb.inf matches USB\\MS_COMP_WINUSB"
check done             "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
