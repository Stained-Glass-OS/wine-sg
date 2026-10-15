#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Devices.Geolocation over GeoClue (patch 3555): a fake GeoClue2 on a private session bus.
#   - position, status, cache, access request and events through the real D-Bus path
#   - consent denied at HKLM, HKCU and HKCU\...\NonPackaged: no D-Bus method call is made at all
#   - GeoClue absent: NotAvailable and the timeout error
#
#   WINE=/path/to/build/wine test/fastb-geo-gate.sh      SG_XVFB_N=261 to name the display
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
T=$(mktemp -d /var/tmp/sg-fastb-geo.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
BUSPID=; FAKEPID=
cleanup() {
    [ -n "$FAKEPID" ] && kill "$FAKEPID" 2>/dev/null
    [ -n "$BUSPID" ] && kill "$BUSPID" 2>/dev/null
    "$WINESERVER" -k 2>/dev/null; rm -rf "$T"
}
trap cleanup EXIT INT TERM

cc -O1 -o "$T/geofake" "$HERE/fastb-geo-fake.c" $(pkg-config --cflags --libs dbus-1) || { echo "FAIL  fake did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastb-geo-probe.c" -lole32 -luuid -lruntimeobject -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }

# private session bus
dbus-daemon --session --address="unix:path=$T/bus" --nofork --nopidfile >/dev/null 2>&1 &
BUSPID=$!
i=0; while [ ! -S "$T/bus" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
[ -S "$T/bus" ] || { echo "FAIL  no private bus"; exit 1; }
export WINE_SG_GEOCLUE_BUS_ADDRESS="unix:path=$T/bus"

mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for d in x86_64-windows; do :; done
[ -e "$WINEPREFIX/drive_c/windows/system32/geolocation.dll" ] || \
    cp "$(dirname "$WINE")/dlls/geolocation/x86_64-windows/geolocation.dll" "$WINEPREFIX/drive_c/windows/system32/" 2>/dev/null
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"

start_fake() { : > "$T/log"; "$T/geofake" "$WINE_SG_GEOCLUE_BUS_ADDRESS" "$T/log" $1 >"$T/fake.out" 2>&1 & FAKEPID=$!
    i=0; while ! grep -q ready "$T/fake.out" 2>/dev/null && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done; }
stop_fake() { [ -n "$FAKEPID" ] && kill "$FAKEPID" 2>/dev/null; wait "$FAKEPID" 2>/dev/null; FAKEPID=; }

cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 120 "$WINE" probe.exe "\$1" 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"

FAIL=0
count() { grep -c -- "$1" "$T/log"; }
expect() { # description, value, expected
    if [ "$2" = "$3" ]; then echo "ok    $1"; else echo "FAIL  $1 (got $2, expected $3)"; FAIL=1; fi
}
phase() { # phase name; the probe output is shown on failure
    XN=${SG_XVFB_N:+-n $SG_XVFB_N}; [ -n "$XN" ] || XN=-a
    timeout -s KILL 200 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh" "$1"
    if grep -qx 'RESULT: PASS' "$T/probe.out"; then echo "ok    probe phase $1"; else echo "FAIL  probe phase $1"; cat "$T/probe.out"; FAIL=1; fi
}

start_fake ""
phase allow
expect "two clients created (default and high accuracy)" "$(count GetClient)" 2
expect "client identifies itself as wine" "$(count 'Set DesktopId=wine')" 2
expect "default accuracy requested once" "$(count 'Set RequestedAccuracyLevel=5')" 1
expect "exact accuracy requested once" "$(count 'Set RequestedAccuracyLevel=8')" 1
expect "clients started" "$(count Start)" 2
expect "clients stopped" "$(count Stop)" 2
stop_fake

start_fake noalt
phase allow-noalt
stop_fake

start_fake ""
phase events
stop_fake

# DesiredAccuracyInMeters, Geopoint (patch 3670): 50 m asks GeoClue for the exact level, 5000 m for the default one
start_fake ""
phase scalar
expect "scalar: 50 m then 5000 m ask for the exact then the default level" "$(grep -- 'Set RequestedAccuracyLevel' "$T/log" | sed 's/.*=//' | tr '\n' ' ')" "8 5 "
stop_fake

# AllowFallbackToConsentlessPositions: one city-level (4) request, none while a switch is off
start_fake ""
phase fallback
expect "fallback: GeoClue asked once, for the city level only" "$(grep -- 'Set RequestedAccuracyLevel' "$T/log" | sed 's/.*=//' | tr '\n' ' ')" "4 "
stop_fake

for p in deny-hklm deny-hkcu deny-np; do
    start_fake ""
    phase $p
    expect "$p: GeoClue was never called" "$(wc -c < "$T/log" | tr -d ' ')" 0
    stop_fake
done

# the bus is up but nothing owns org.freedesktop.GeoClue2
: > "$T/log"
phase absent
phase default

[ $FAIL = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $FAIL
