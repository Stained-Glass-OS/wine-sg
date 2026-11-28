#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Programs hear the default output change (patches/sg/9419).
#
# Windows tells programs when the person picks another default output or input
# (IMMNotificationClient::OnDefaultDeviceChanged, for each role, with the default
# device's ID); players, games and voice chat follow it there. Wine told them only
# when its own registry default changed (winecfg), never when the sound server's
# did (the volume flyout, Settings > Sound).
#
# On a private PipeWire with two null sinks (no device): test/defdev-probe.c
# registers a notification client, with no stream and then with one playing, and
# has the default output (then the default input) changed with pactl.
#
#   WINE=/opt/wine-sg/bin/wine test/defdev-gate.sh
# Mutant: SG_MUTANT_PULSE_NO_DEFAULT_NOTIFY (winepulse.drv pulse.c)
# Skips (77) without PipeWire, WirePlumber, pactl or mingw.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for need in pipewire wireplumber pactl awk "$MINGW"; do
    command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
case "$XDG_RUNTIME_DIR" in /run/user/*|'') echo "refusing: XDG_RUNTIME_DIR is '$XDG_RUNTIME_DIR'"; exit 1 ;; esac

T=$(mktemp -d /var/tmp/sg-defdev.XXXXXX)
PIDS=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$T/prefix"
    for p in $PIDS; do kill "$p" 2>/dev/null; done
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -O1 -o "$T/defdev-probe.exe" "$HERE/defdev-probe.c" -lole32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }

# WirePlumber keeps programs' volumes (restore-stream) under XDG_STATE_HOME: the gate's own
export XDG_STATE_HOME="$T/state"
# --- the private sound server ---------------------------------------------------------
mkdir -p "$T/pw" "$T/wp/wireplumber.conf.d"
cat > "$T/pw/pipewire.conf" <<'EOF'
context.properties = { core.daemon = true core.name = pipewire-0 default.clock.rate = 48000
    default.clock.quantum = 1024 default.clock.min-quantum = 32 default.clock.max-quantum = 2048 }
context.spa-libs = { audio.convert.* = audioconvert/libspa-audioconvert audio.adapt = audioconvert/libspa-audioconvert
    support.* = support/libspa-support }
context.modules = [
    { name = libpipewire-module-rt args = { nice.level = -11 rt.prio = 88 } flags = [ ifexists nofail ] }
    { name = libpipewire-module-protocol-native } { name = libpipewire-module-profiler }
    { name = libpipewire-module-metadata } { name = libpipewire-module-spa-node-factory }
    { name = libpipewire-module-client-node } { name = libpipewire-module-access }
    { name = libpipewire-module-adapter } { name = libpipewire-module-link-factory }
    { name = libpipewire-module-protocol-pulse }
]
pulse.properties = { server.address = [ "unix:native" ] }
context.objects = [
    { factory = metadata args = { metadata.name = default } }
    { factory = spa-node-factory args = { factory.name = support.node.driver node.name = Dummy-Driver
        node.group = pipewire.dummy priority.driver = 20000 } }
    { factory = adapter args = { factory.name = support.null-audio-sink node.name = sgnull
        media.class = Audio/Sink object.linger = true audio.position = [ FL FR ] audio.rate = 48000
        node.driver = true priority.driver = 30000 priority.session = 3000 } }
]
EOF
cat > "$T/wp/wireplumber.conf.d/90-gate.conf" <<'EOF'
wireplumber.profiles = { main = { monitor.alsa = disabled monitor.alsa-midi = disabled monitor.bluez = disabled
    monitor.bluez.seat-monitoring = disabled monitor.v4l2 = disabled monitor.libcamera = disabled
    support.logind = disabled support.reserve-device = disabled support.portal-permissionstore = disabled } }
EOF
export PIPEWIRE_RUNTIME_DIR="$XDG_RUNTIME_DIR" PULSE_RUNTIME_PATH="$XDG_RUNTIME_DIR/pulse"
export PULSE_SERVER="unix:$XDG_RUNTIME_DIR/pulse/native" DBUS_SESSION_BUS_ADDRESS=disabled:
PIPEWIRE_CONFIG_DIR="$T/pw" pipewire > "$T/pw.log" 2>&1 & PIDS="$PIDS $!"
i=0; while [ ! -S "$XDG_RUNTIME_DIR/pipewire-0" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
WIREPLUMBER_CONFIG_DIR="$T/wp:/usr/share/wireplumber" XDG_CONFIG_HOME="$T/wpcfg" wireplumber > "$T/wp.log" 2>&1 & PIDS="$PIDS $!"
i=0; while ! pactl info >/dev/null 2>&1 && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
pactl set-default-sink sgnull 2>/dev/null
pactl list short sinks 2>/dev/null | grep -q sgnull || { echo "FAIL  the private sound server did not start"; tail -5 "$T/pw.log"; exit 1; }
pactl load-module module-null-sink sink_name=sgnull2 sink_properties=device.description=SGnull2 >/dev/null

# --- the program and the mixer ----------------------------------------------------------
export WINEPREFIX="$T/prefix" WINEDLLOVERRIDES="$WINEDLLOVERRIDES;mscoree,mshtml=" DISPLAY= WAYLAND_DISPLAY=sg-gate-no-wayland
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/defdev-probe.exe" "$WINEPREFIX/drive_c/"
# to a file: services started with the prefix keep a pipe open
(cd "$WINEPREFIX/drive_c" && WINEDEBUG=-all timeout -s KILL 120 "$WINE" defdev-probe.exe > "$T/probe.out" 2>/dev/null)
out=$(tr -d '\r' < "$T/probe.out")
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check registered   "a notification client registers"
check told         "the default output changes: the program is told"
check roles        "for every role (multimedia, communications)"
check id           "with the default device's ID"
check told_playing "and with a stream playing"
check capture      "the default input changes: told"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
