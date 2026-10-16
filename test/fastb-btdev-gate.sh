#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# bluetoothapis remote devices over winebth.sys (patch 3567): a fake BlueZ (one adapter, two devices) on a private
# D-Bus; the system bus address is pointed at it before the prefix boots, so winebth.sys talks to the fake.
#   - BluetoothFindFirst/Next/CloseDevice with every filter, GetDeviceInfo, EnumerateInstalledServices,
#     SetServiceState, UpdateDeviceRecord, EnableDiscovery, RemoveDevice, checked against the fake's call log
#
#   WINE=/path/to/build/wine test/fastb-btdev-gate.sh      SG_XVFB_N=261 to name the display
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
command -v dbus-daemon >/dev/null || { echo "SKIP: needs dbus-daemon"; exit 77; }
pkg-config --exists dbus-1 || { echo "SKIP: needs libdbus headers"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-btdev.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
BUSPID=; FAKEPID=
cleanup() {
    [ -n "$FAKEPID" ] && kill "$FAKEPID" 2>/dev/null
    "$WINESERVER" -k 2>/dev/null
    [ -n "$BUSPID" ] && kill "$BUSPID" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

cc -O1 -o "$T/btfake" "$HERE/fastb-btdev-fake.c" $(pkg-config --cflags --libs dbus-1) || { echo "FAIL  fake did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastb-btdev-probe.c" -lbluetoothapis || \
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastb-btdev-probe.c" -lbthprops -lbluetoothapis 2>/dev/null || { echo "FAIL  probe did not build"; exit 1; }

dbus-daemon --session --address="unix:path=$T/bus" --nofork --nopidfile >/dev/null 2>&1 &
BUSPID=$!
i=0; while [ ! -S "$T/bus" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
[ -S "$T/bus" ] || { echo "FAIL  no private bus"; exit 1; }
: > "$T/log"
"$T/btfake" "unix:path=$T/bus" "$T/log" >"$T/fake.out" 2>&1 &
FAKEPID=$!
i=0; while ! grep -q ready "$T/fake.out" 2>/dev/null && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
grep -q ready "$T/fake.out" || { echo "FAIL  fake did not start"; exit 1; }
export DBUS_SYSTEM_BUS_ADDRESS="unix:path=$T/bus"

mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# the radio comes up a moment after the driver starts
cat > "$T/run.sh" <<EOS
#!/bin/sh
i=0
while [ \$i -lt 40 ]; do
  timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>&1 </dev/null | tr -d '\r' > "$T/probe.out"
  grep -q "no radio" "$T/probe.out" || break
  sleep 1; i=\$((i+1))
done
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 300 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
FAIL=0
grep -q "^RESULT: PASS" "$T/probe.out" || FAIL=1
expect() { # description, pattern
    if grep -q -- "$2" "$T/log"; then echo "ok    $1"; else echo "FAIL  $1"; FAIL=1; fi
}
expect "A2DP connected"        'ConnectProfile .*dev_AA_BB_CC_DD_EE_01 0000110b-0000-1000-8000-00805f9b34fb'
expect "HFP disconnected"      'DisconnectProfile .*dev_AA_BB_CC_DD_EE_01 0000111e-0000-1000-8000-00805f9b34fb'
expect "alias set"             'Set .*dev_AA_BB_CC_DD_EE_01 Alias=Renamed Headset'
expect "discoverable on"       'Set /org/bluez/hci0 Discoverable=1'
expect "discoverable off"      'Set /org/bluez/hci0 Discoverable=0'
expect "device removed"        'RemoveDevice /org/bluez/hci0/dev_11_22_33_44_55_66'
[ $FAIL = 0 ] && echo "RESULT: PASS"
exit $FAIL
