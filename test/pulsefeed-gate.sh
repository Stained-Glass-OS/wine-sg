#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winepulse hands what a program has written to the sound server at once, up to 100 ms (patches/sg/8600).
#
# Media Player's sound hiccuped where VLC's did not (David's 1080p renders, 2026-11-22): winepulse
# kept three periods (30 ms) at the sound server and the rest in its own buffer, fed by its timer
# thread -- which waits on the sound server and the wineserver, 20 to 60 ms at a time beside a film
# decoding, and the server ran dry. Here: a private PipeWire (one null sink, the pulse protocol, and
# WirePlumber with every device monitor off: nothing reaches a real sound card) under the gate's own
# runtime directory; test/pulsefeed-probe.c plays a tone through DirectSound; every 400 ms the whole
# Wine process is stopped for 30 ms (SIGSTOP), as a busy machine stalls it. Then:
#   - the sound server never ran dry (winepulse's underflow callback, warn+pulse), and
#   - the null sink's monitor, recorded, carries the tone with no gap of silence.
# Mutant: winepulse built with -DSG_MUTANT_PULSE_SHORT_TLENGTH (30 ms, as before) fails both.
#
#   WINE=/opt/wine-sg/bin/wine test/pulsefeed-gate.sh
# Skips (77) without PipeWire, WirePlumber, pactl/parec or mingw.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
STALL_MS="${PULSEFEED_STALL_MS:-30}"
SECS=12
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for need in pipewire wireplumber pactl parec python3 "$MINGW"; do
    command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
case "$XDG_RUNTIME_DIR" in /run/user/*|'') echo "refusing: XDG_RUNTIME_DIR is '$XDG_RUNTIME_DIR'"; exit 1 ;; esac

T=$(mktemp -d /var/tmp/sg-pulsefeed.XXXXXX)
PIDS=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    for p in $PIDS; do kill "$p" 2>/dev/null; done
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -O1 -o "$T/pulsefeed-probe.exe" "$HERE/pulsefeed-probe.c" -ldsound -lole32 -luuid -lm \
    || { echo "FAIL  probe did not build"; exit 1; }

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

# --- the tone, with the process stalled now and then -------------------------------------
export WINEPREFIX="$T/prefix" WINEDLLOVERRIDES="$WINEDLLOVERRIDES;mscoree,mshtml=" DISPLAY= WAYLAND_DISPLAY=sg-gate-no-wayland
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/pulsefeed-probe.exe" "$WINEPREFIX/drive_c/"
(cd "$WINEPREFIX/drive_c" && WINEDEBUG=+timestamp,warn+pulse exec "$WINE" pulsefeed-probe.exe "$SECS" > "$T/probe.out" 2> "$T/probe.err") &
i=0; while ! grep -q playing "$T/probe.out" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.25; i=$((i + 1)); done
grep -q playing "$T/probe.out" || { fail "the probe did not play: $(head -3 "$T/probe.out" "$T/probe.err" | tr '\n' ' ')"; exit 1; }
sleep 1
probe=$(pgrep -n -f 'pulsefeed-probe.exe' 2>/dev/null)
[ -n "$probe" ] || { fail "no probe process"; exit 1; }
start=$(grep -c pulse_underflow_callback "$T/probe.err")
parec -d sgnull.monitor --format=s16le --rate=48000 --channels=2 > "$T/rec.raw" & REC=$!; PIDS="$PIDS $REC"
python3 - "$probe" "$STALL_MS" <<'EOF'
import os, signal, sys, time
pid, ms = int(sys.argv[1]), int(sys.argv[2])
end = time.monotonic() + 8
while time.monotonic() < end:
    time.sleep(0.4)
    try:
        os.kill(pid, signal.SIGSTOP); time.sleep(ms / 1000.0); os.kill(pid, signal.SIGCONT)
    except ProcessLookupError:
        break
EOF
sleep 0.5; kill "$REC" 2>/dev/null
under=$(( $(grep -c pulse_underflow_callback "$T/probe.err") - start ))
gaps=$(python3 - "$T/rec.raw" <<'EOF'
import sys, array
a = array.array('h'); a.frombytes(open(sys.argv[1], 'rb').read()); x = a[0::2]
B = 240                                   # 5 ms blocks
peaks = [max(abs(v) for v in x[i:i + B]) for i in range(0, len(x) - B, B)]
act = [i for i, p in enumerate(peaks) if p > 2000]
if not act: print("silent"); sys.exit()
print(sum(1 for p in peaks[act[0]:act[-1]] if p < 500))
EOF
)
echo "      stalls of ${STALL_MS} ms every 400 ms for 8 s: $under underflow(s), recording gaps (5 ms blocks): $gaps"
[ "$gaps" != silent ] && pass "the tone reaches the sound server" || fail "nothing was recorded from the sound server"
[ "$under" = 0 ] && pass "the sound server never ran dry while the program stalled" \
    || fail "the sound server ran dry $under time(s) while the program stalled"
[ "$gaps" = 0 ] && pass "the tone has no gap" || fail "the tone has $gaps gap block(s)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
