#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Media Foundation opens file URLs (patches/sg/9420).
#
# The file scheme handler cut "file://" off a URL and took the rest for a path:
# file:///C:/... became "/C:/...", with %20 for each space -- no file. Players
# and the media engine handed a file URL (a .NET or WinRT Uri's) failed to open
# it. A file URL is now the path it names, as a URL's rules have it.
#
# test/mfurl-probe.c writes C:\sg media\tone.wav, opens it with the source
# resolver under five spellings and plays the file:/// one through the media
# engine (on a private PipeWire with a null sink: nothing reaches a sound card).
#
#   WINE=/opt/wine-sg/bin/wine test/mfurl-gate.sh
# Mutant: SG_MUTANT_FILEURL_CUT_ONLY (mf scheme_handler.c)
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

T=$(mktemp -d /var/tmp/sg-mfurl.XXXXXX)
PIDS=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$T/prefix"
    for p in $PIDS; do kill "$p" 2>/dev/null; done
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -O1 -o "$T/mfurl-probe.exe" "$HERE/mfurl-probe.c" -lole32 -loleaut32 -luuid -lmfplat -lmfuuid \
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

# --- the program and the mixer ----------------------------------------------------------
export WINEPREFIX="$T/prefix" WINEDLLOVERRIDES="$WINEDLLOVERRIDES;mscoree,mshtml=" DISPLAY= WAYLAND_DISPLAY=sg-gate-no-wayland
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/mfurl-probe.exe" "$WINEPREFIX/drive_c/"
# to a file: services started with the prefix keep a pipe open
(cd "$WINEPREFIX/drive_c" && WINEDEBUG=-all timeout -s KILL 120 "$WINE" mfurl-probe.exe > "$T/probe.out" 2>/dev/null)
out=$(tr -d '\r' < "$T/probe.out")
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check write         "the probe wrote its WAV"
check url_escaped   "file:///C:/sg%20media/tone.wav opens"
check url_spaces    "file:///C:/sg media/tone.wav opens"
check url_backslash "file:///C:\\sg media\\tone.wav opens"
check url_drive     "file://C:/sg media/tone.wav still opens"
check path          "C:\\sg media\\tone.wav still opens"
check engine        "the media engine plays the file:/// URL to its end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
