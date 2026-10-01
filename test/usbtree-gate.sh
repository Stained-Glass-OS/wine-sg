#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The USB tree (patches/sg/0643) and the usbscan driver (0642). Scanner
# software (the Ambir ImageScan Pro 490i's) walks \\.\HCD0 -> its root hub ->
# the hub's ports, matches its device there, then opens \\.\UsbscanN. Wine
# had no \\.\HCD0; the library said "not connected". The probe walks the tree
# as it does; the devices are the machine's own (read only: the descriptors
# wineusb already holds).
#
#   WINE=/opt/wine-sg/bin/wine test/usbtree-gate.sh
# Mutation: -DSG_MUTANT_NOUSBTREE (wineusb.sys makes no tree): hcd fails.
# A scan through usbscan needs the scanner: VM 2290 with it passed through.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -d /sys/bus/usb/devices ] || { echo "SKIP: no USB on this machine"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-usbtree.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O2 -o "$T/tree.exe" "$HERE/usbtree-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
# wineusb comes up with the first program; give it a moment to list devices
out=$(timeout 60 "$WINE" "$T/tree.exe" 2>/dev/null | tr -d '\r')
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
[ "$(v hcd)" = 1 ] || { sleep 3; out=$(timeout 60 "$WINE" "$T/tree.exe" 2>/dev/null | tr -d '\r'); }
[ "$(v usbscan)" = 1 ] && pass "usbscan.sys is installed" || fail "no usbscan.sys"
[ "$(v hcd)" = 1 ] && pass "\\\\.\\HCD0 opens" || fail "no \\\\.\\HCD0: $out"
[ "$(v driverkey)" = 1 ] && pass "the controller's driver key name" || fail "driver key: $out"
case "$(v roothub)" in USB#ROOT_HUB*) pass "root hub $(v roothub)" ;; *) fail "root hub name: '$(v roothub)'" ;; esac
[ "$(v hubopen)" = 1 ] && pass "the root hub opens" || fail "the root hub does not open"
[ "${ports:=$(v ports)}" -ge 1 ] 2>/dev/null && pass "$ports ports" || fail "ports: '$ports'"
[ "$(v portsok)" = 1 ] && pass "every port answers connection information" || fail "port queries failed: $out"
if [ "$(v connected)" -ge 1 ] 2>/dev/null; then
    [ "$(v descok)" = "$(v connected)" ] && pass "$(v connected) devices, each port's device descriptor matches" ||
        fail "descriptors: $(v descok) of $(v connected)"
else
    echo "NOTE  no USB device wineusb can see here; descriptors not checked"
fi
exit $RC
