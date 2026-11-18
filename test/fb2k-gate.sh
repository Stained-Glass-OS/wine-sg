#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# fb2k-gate.sh: foobar2000 2.x (x64) works (7850). It is a C++/CX program in part
# (PP-UWP-Interop.dll), which needs vccorlib140's Platform::Details::Heap,
# ControlBlock::ReleaseTarget, Platform::Delegate, GetIidsFn and
# __abi_translateCurrentException: without them it stops at start with "unimplemented
# function vccorlib140.DLL.?Allocate@Heap@Details@Platform@@SAPEAX_K0@Z".
# The gate installs it silently into a fresh prefix, starts it on its own Xvfb with a
# private PipeWire whose default sink is a null sink, adds a FLAC, an MP3 and an Opus
# file, plays (the title is the track's, the position advances, a stream reaches the
# sink), goes to the next tracks, opens Preferences and exits cleanly.
# Needs the installer (foobar2000.org, not shipped): FB2K_INSTALLER, default
# /var/tmp/sgwork/fb2k/fb2k-x64.exe; skips without it.
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
INST=${FB2K_INSTALLER:-/var/tmp/sgwork/fb2k/fb2k-x64.exe}
RC=0
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
pass() { printf 'PASS  %s\n' "$*"; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f "$INST" ] || { echo "SKIP: no foobar2000 installer at $INST"; exit 77; }
for need in Xvfb xdotool import convert ffmpeg pipewire pipewire-pulse wireplumber pactl dbus-daemon; do
    command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
WD=$(dirname "$WINE")
if [ -x "$WD/wineserver" ]; then WS=$WD/wineserver; else WS=$WD/server/wineserver; fi
T=$(mktemp -d /var/tmp/sg-fb2k.XXXXXX)
PIDS=""
cleanup() { WINEPREFIX=$T/pfx "$WS" -k 2>/dev/null; for p in $PIDS; do kill "$p" 2>/dev/null; done; sleep 1; rm -rf "$T"; }
trap cleanup EXIT INT TERM
export WINEPREFIX=$T/pfx WINESERVER=$WS WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree=d;mshtml=d"
unset WAYLAND_DISPLAY; export WAYLAND_DISPLAY=sg-no-wayland
R=$T/run; mkdir -p "$R"; chmod 700 "$R"
export XDG_RUNTIME_DIR=$R PIPEWIRE_RUNTIME_DIR=$R PULSE_RUNTIME_PATH=$R/pulse
export XDG_CONFIG_HOME=$T/cfg XDG_STATE_HOME=$T/state XDG_DATA_HOME=$T/data
cat > "$R/bus.conf" <<CONF
<busconfig><type>session</type><listen>unix:path=$R/bus</listen>
<policy context="default"><allow send_destination="*" eavesdrop="true"/><allow eavesdrop="true"/><allow own="*"/></policy></busconfig>
CONF
dbus-daemon --config-file="$R/bus.conf" --nofork --nopidfile >/dev/null 2>&1 & PIDS="$PIDS $!"
export DBUS_SESSION_BUS_ADDRESS=unix:path=$R/bus
sleep 0.5
pipewire >"$T/pw.log" 2>&1 & PIDS="$! $PIDS"; sleep 1
wireplumber >"$T/wp.log" 2>&1 & PIDS="$! $PIDS"
pipewire-pulse >"$T/pwp.log" 2>&1 & PIDS="$! $PIDS"; sleep 2
pactl load-module module-null-sink sink_name=sgnull >/dev/null && pactl set-default-sink sgnull \
    || { echo "SKIP: no private PipeWire null sink"; exit 77; }
Xvfb -displayfd 3 -screen 0 1600x900x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & PIDS="$! $PIDS"
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
export DISPLAY=":$(cat "$T/display")"

M=$HOME/Music-fb2k; mkdir -p "$M"
ffmpeg -loglevel error -y -f lavfi -i "sine=frequency=440:duration=30" -metadata title="Tone A" -metadata artist="SG Test" "$M/a.flac"
ffmpeg -loglevel error -y -f lavfi -i "sine=frequency=660:duration=30" -metadata title="Tone B" -metadata artist="SG Test" -c:a libmp3lame "$M/b.mp3"
ffmpeg -loglevel error -y -f lavfi -i "sine=frequency=880:duration=30" -metadata title="Tone C" -metadata artist="SG Test" -c:a libopus "$M/c.opus"

timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WS" -w
cp "$INST" "$T/fb2k-setup.exe"
timeout -s KILL 240 "$WINE" "$T/fb2k-setup.exe" /S >/dev/null 2>&1; "$WS" -w
EXE=$(find "$WINEPREFIX/drive_c" -iname foobar2000.exe | head -1)
[ -n "$EXE" ] && pass "the silent install puts foobar2000.exe in place" || { fail "foobar2000 did not install"; exit 1; }

title() { xdotool search --onlyvisible --name '\[foobar2000\]|^foobar2000 v' 2>/dev/null | head -1 | xargs -r xdotool getwindowname 2>/dev/null; }
fb() { timeout -s KILL 30 "$WINE" "$EXE" "$@" >/dev/null 2>&1; }
wait_title() { i=0; while [ $i -lt "$2" ]; do case "$(title)" in *"$1"*) return 0;; esac; sleep 1; i=$((i+1)); done; return 1; }
A=$("$WINE" winepath -w "$M/a.flac" | tr -d '\r'); B=$("$WINE" winepath -w "$M/b.mp3" | tr -d '\r'); C=$("$WINE" winepath -w "$M/c.opus" | tr -d '\r')

WINEDEBUG=err+all,fixme-all "$WINE" "$EXE" >"$T/run.log" 2>&1 & FB=$!
if wait_title foobar2000 60; then pass "foobar2000 starts: its main window ($(title))"
else fail "no foobar2000 main window"; grep -m3 -i "unimplemented\|err:" "$T/run.log"; exit 1; fi
fb /add "$A" "$B" "$C"; sleep 3
fb /play
if wait_title "Tone A" 20; then pass "FLAC plays: the title is the track's ($(title))"; else fail "FLAC: title '$(title)'"; fi

# the position advances: the status bar (bottom of the main window) changes over two seconds
win=$(xdotool search --onlyvisible --name 'foobar2000' | head -1)
eval "$(xdotool getwindowgeometry --shell "$win")"
strip() { import -window root -crop "${WIDTH}x30+${X}+$((Y + HEIGHT - 30))" "$T/$1.png" 2>/dev/null; }
sleep 2; strip pos1; sleep 2; strip pos2
diff=$(compare -metric AE "$T/pos1.png" "$T/pos2.png" null: 2>&1 | cut -d' ' -f1)
[ "${diff%.*}" -gt 0 ] 2>/dev/null && pass "the playback position advances ($diff pixels of the status bar changed)" \
    || fail "the status bar did not change while playing ($diff)"
n=$(pactl list short sink-inputs | wc -l)
[ "$n" -ge 1 ] && pass "an audio stream reaches the (null) sink" || fail "no audio stream at the sink"

fb /next
wait_title "Tone B" 15 && pass "MP3 plays: next track ($(title))" || fail "MP3: title '$(title)'"
fb /next
wait_title "Tone C" 15 && pass "Opus plays: next track ($(title))" || fail "Opus: title '$(title)'"

fb "/command:Preferences"
i=0; while [ $i -lt 15 ] && ! xdotool search --onlyvisible --name '^Preferences' >/dev/null 2>&1; do sleep 1; i=$((i+1)); done
xdotool search --onlyvisible --name '^Preferences' >/dev/null 2>&1 && pass "Preferences opens" || fail "no Preferences window"
pw=$(xdotool search --onlyvisible --name '^Preferences' | head -1)
[ -n "$pw" ] && { xdotool windowactivate --sync "$pw" key Escape 2>/dev/null; sleep 2; }

fb /exit
i=0; while [ $i -lt 30 ] && kill -0 $FB 2>/dev/null; do sleep 1; i=$((i+1)); done
kill -0 $FB 2>/dev/null && fail "foobar2000 did not exit" || pass "/exit ends foobar2000"
if grep -q "unimplemented function\|Unhandled exception\|page fault" "$T/run.log"; then
    fail "errors in its log: $(grep -m2 "unimplemented function\|Unhandled exception\|page fault" "$T/run.log" | tr '\n' ' ')"
else pass "no unimplemented function, unhandled exception or page fault in its log"; fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
