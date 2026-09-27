#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0445: a battery that reports energy (uWh, uW) rather than
# charge (uAh, uA) -- ThinkPads among many -- reads right in
# GetSystemPowerStatus: the taskbar's battery icon and every program's power
# status. Fake batteries are bind-mounted over /sys/class/power_supply in a
# private mount namespace (needs sudo -n; 77 without).
#   WINE=... test/battery-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
W=$(mktemp -d /var/tmp/battery-gate.XXXXXX); chmod 755 "$W"
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
sudo -n true 2>/dev/null && command -v unshare >/dev/null || { echo "SKIP: needs sudo -n and unshare"; exit 77; }
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
cleanup() { set +e; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; rm -rf "$W"; }
trap cleanup EXIT
"$MINGW" -O2 -o "$W/probe.exe" "$HERE/battery-probe.c" || { fail "probe did not build"; exit 1; }
export WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY
"$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -k 2>/dev/null

ps() { # dir -> the probe's line, with that directory as /sys/class/power_supply
    sudo -n unshare -m sh -c "mount --bind '$1' /sys/class/power_supply && exec sudo -n -u '$(id -un)' env HOME='$HOME' \
        WINEPREFIX='$WINEPREFIX' WINEDEBUG=-all '$WINE' '$W/probe.exe'" 2>/dev/null | tr -d '\r'
    WINEPREFIX=$WINEPREFIX "$WINESERVER" -k 2>/dev/null; sleep 0.5
}
mk() { # dir kind(energy|charge) status ac
    mkdir -p "$1/BAT0" "$1/AC"
    printf 'Battery\n' > "$1/BAT0/type"; printf '%s\n' "$3" > "$1/BAT0/status"; printf '11400000\n' > "$1/BAT0/voltage_now"
    if [ "$2" = energy ]; then
        printf '50000000\n' > "$1/BAT0/energy_full"; printf '31500000\n' > "$1/BAT0/energy_now"; printf '10000000\n' > "$1/BAT0/power_now"
    else
        printf '4385965\n' > "$1/BAT0/charge_full"; printf '2763158\n' > "$1/BAT0/charge_now"; printf '877192\n' > "$1/BAT0/current_now"
    fi
    printf 'Mains\n' > "$1/AC/type"; printf '%s\n' "$4" > "$1/AC/online"
}
mk "$W/energy" energy Discharging 0
out=$(ps "$W/energy")
echo "      energy: $out"
set -- $out
[ "${6:-}" = 63 ] && pass "a battery reporting energy: 63% (it read as empty of capacity)" || fail "energy percent: $out"
[ "${2:-}" = 0 ] && [ "${8:-0}" -gt 10800 ] && [ "${8:-0}" -lt 12000 ] && pass "on battery, about 3 h 9 min left ($8 s)" || fail "energy time: $out"
mk "$W/charge" charge Discharging 0
out=$(ps "$W/charge")
echo "      charge: $out"
set -- $out
[ "${6:-}" = 63 ] && pass "a battery reporting charge still reads right: 63%" || fail "charge percent: $out"
mk "$W/plugged" energy Charging 1
out=$(ps "$W/plugged")
echo "      plugged in: $out"
set -- $out
[ "${2:-}" = 1 ] && [ $(( ${4:-0} & 8 )) = 8 ] && pass "plugged in and charging" || fail "charging: $out"
echo "battery-gate: $fails failure(s)"
[ "$fails" = 0 ]
