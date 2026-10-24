#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A program talks to a USB device through WinUSB (patches/sg/6519-6520): the device declares
# WinUSB in its Microsoft OS descriptors, wineusb reports USB\MS_COMP_WINUSB, winusb.sys takes
# it and registers the interface GUID it names, and test/winusb-probe.c finds it with SetupDi,
# opens it and uses the WinUSB API (descriptors, control and bulk transfers both ways,
# overlapped I/O, a pipe timeout, abort and reset).
# The device is the test gadget of test/usbgadget-setup.sh, which needs root and a test VM:
#   sudo test/usbgadget-setup.sh start "$USER"; make test-winusb; sudo test/usbgadget-setup.sh stop
# A VM without mingw can use a probe built elsewhere: PROBE=/path/winusb-probe.exe.
# Without the gadget this SKIPs. Mutants (host builds only; not run on a VM): SG_MUTANT_WINEUSB_NO_MSOS (wineusb.sys unixlib.c),
# SG_MUTANT_WINEUSB_NO_CONTROL (wineusb.sys unixlib.c), SG_MUTANT_WINUSB_NOT_STARTED (winusb.sys).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
PROBE="${PROBE:-}"
[ -n "$PROBE" ] || command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing (or PROBE=prebuilt winusb-probe.exe)"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
node=
for d in /sys/bus/usb/devices/*; do
    [ -f "$d/idVendor" ] || continue
    if [ "$(cat "$d/idVendor")" = 1209 ] && [ "$(cat "$d/idProduct")" = 0001 ]; then
        node=$(printf '/dev/bus/usb/%03d/%03d' "$(cat "$d/busnum")" "$(cat "$d/devnum")")
    fi
done
[ -n "$node" ] || { echo "SKIP: no WinUSB test gadget (sudo test/usbgadget-setup.sh start \$USER, on a test VM)"; exit 77; }
[ -r "$node" ] && [ -w "$node" ] || { echo "SKIP: $node is not usable by $(id -un) (usbgadget-setup.sh start $(id -un))"; exit 77; }
T=$(mktemp -d /var/tmp/sg-winusb.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
if [ -n "$PROBE" ]; then
    cp "$PROBE" "$T/probe.exe" || { fail "no probe at $PROBE"; exit 1; }
else
    TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/winusb-probe.c" -lsetupapi -luuid || { fail "probe did not build"; exit 1; }
fi
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
check found             "the device interface the gadget names is there (SetupDi)"
check open              "the device opens"
check initialize        "WinUsb_Initialize"
check speed             "its speed"
check interface         "its interface: vendor class, two endpoints"
check pipes             "its pipes: bulk IN 0x81, bulk OUT 0x02"
check no_associated     "no associated interface"
check device_descriptor "the device descriptor (1209:0001)"
check string_descriptor "a string descriptor"
check control_in        "a vendor IN request"
check control_out       "a vendor OUT request"
check control_roundtrip "what it wrote comes back"
check control_stall     "an unknown request fails"
check write             "WinUsb_WritePipe"
check echo              "WinUsb_ReadPipe reads the echo"
check pattern           "and 1024 bytes of a known pattern"
check overlapped        "an overlapped read completes with what was written"
check timeout           "a pipe timeout ends a read with ERROR_SEM_TIMEOUT"
check abort             "WinUsb_AbortPipe ends a pending read"
check reset             "WinUsb_ResetPipe"
check after_reset       "the pipes work after it"
check free              "WinUsb_Free"
check done              "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
