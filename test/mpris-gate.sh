#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The transport controls on the desktop (patches/sg/6523): an enabled SystemMediaTransportControls
# session is an MPRIS player on the user's session bus, with the display updater's metadata and
# cover (a file:// URL in the user's cache), PlaybackStatus and the Can* flags from IsXEnabled;
# the bus's Play/Pause/PlayPause/Next/Previous/Stop come back as ButtonPressed
# (test/mpris-probe.c). The gate runs its own bus (dbus-run-session) and its own X server.
#   WINE=/opt/wine-sg/bin/wine test/mpris-gate.sh
# A gate for a 32-bit program too: MINGW=i686-w64-mingw32-gcc.
# With a timeline (6525): CanSeek, Position, Seek and SetPosition as PlaybackPositionChangeRequested,
# Seeked when the program jumps.
# Mutants: SG_MUTANT_MPRIS_NO_SEEK (unixlib.c: no position requests), SG_MUTANT_MPRIS_NO_SIGNAL (unixlib.c: no PropertiesChanged), SG_MUTANT_MPRIS_NO_BUTTON
# (main.c: no ButtonPressed), SG_MUTANT_SMTC_NO_APARTMENT (main.c: not agile, called on our thread)
# (windows.media.mediacontrol); SG_MUTANT_REF_NO_FOREIGN (windows.storage
# stream.c: a reference to another implementation's stream could not be opened: no cover).
set -u
if [ -z "${SG_MPRIS_BUS:-}" ]; then
    command -v dbus-run-session >/dev/null || { echo "SKIP: dbus-run-session missing"; exit 77; }
    unset DBUS_SESSION_BUS_ADDRESS
    SG_MPRIS_BUS=1 exec dbus-run-session -- sh "$0" "$@"
fi
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=; PP=; MP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb gdbus dbus-monitor "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ] || { fail "no session bus from dbus-run-session"; exit 1; }
T=$(mktemp -d /var/tmp/sg-mpris.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; for p in $XP $PP $MP; do kill "$p" 2>/dev/null; done; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/mpris-probe.c" -lole32 -luuid -lruntimeobject -luser32 || { fail "probe did not build"; exit 1; }
# (SG_MPRIS_DISPLAY=N: that display number; otherwise the X server picks a free one)
if [ -n "${SG_MPRIS_DISPLAY:-}" ]; then
    Xvfb ":$SG_MPRIS_DISPLAY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -e "/tmp/.X11-unix/X$SG_MPRIS_DISPLAY" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    echo "$SG_MPRIS_DISPLAY" > "$T/display"
else
    Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG="${SG_MPRIS_DEBUG:--all}" WINESERVER XDG_CACHE_HOME="$T/cache"
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
C="$WINEPREFIX/drive_c"

names() { gdbus call --session -d org.freedesktop.DBus -o /org/freedesktop/DBus -m org.freedesktop.DBus.ListNames 2>/dev/null |
          grep -o "org\.mpris\.MediaPlayer2\.[A-Za-z0-9_.]*" ; }
prop() { gdbus call --session -d "$NAME" -o /org/mpris/MediaPlayer2 -m org.freedesktop.DBus.Properties.Get "$1" "$2" 2>&1; }
call() { gdbus call --session -d "$NAME" -o /org/mpris/MediaPlayer2 -m "org.mpris.MediaPlayer2.Player.$1" >/dev/null 2>&1; }
stage() { i=0; while ! tr -d '\r' < "$T/raw" | grep -q "^stage=$1" && [ $i -lt 600 ]; do sleep 0.1; i=$((i + 1)); done
          tr -d '\r' < "$T/raw" > "$T/out"; grep -q "^stage=$1" "$T/out"; }
yes_no() { if [ "$1" = 1 ]; then pass "$2"; else fail "$2${3:+ ($3)}"; fi; }

timeout -s KILL 240 "$WINE" 'C:\probe.exe' > "$T/raw" 2>"${SG_MPRIS_LOG:-/dev/null}" & PP=$!
stage disabled || { fail "the probe did not start"; cat "$T/out"; exit 1; }
sleep 0.5
yes_no "$([ -z "$(names)" ] && echo 1)" "a session not yet enabled is not on the bus" "$(names)"
stage ready || { fail "the probe did not publish"; cat "$T/out"; exit 1; }
i=0; while [ -z "$(names)" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
NAME=$(names | head -n 1)
PID=$(gdbus call --session -d org.freedesktop.DBus -o /org/freedesktop/DBus -m org.freedesktop.DBus.GetConnectionUnixProcessID "$NAME" 2>/dev/null | grep -o '[0-9][0-9]*' | tail -n 1)
printf '      bus name: %s (owner pid %s)\n' "$NAME" "$PID"
yes_no "$(printf '%s' "$NAME" | grep -Eq "^org\.mpris\.MediaPlayer2\.probe\.instance${PID}_[0-9]+$" && echo 1)" \
    "the enabled session is org.mpris.MediaPlayer2.<program>.instance<its pid>_<n>" "$NAME"
[ -n "$NAME" ] || { cat "$T/out"; exit 1; }
ID=$(prop org.mpris.MediaPlayer2 Identity)
yes_no "$(printf '%s' "$ID" | grep -q "'probe'" && echo 1)" "Identity is the program's name" "$ID"
MD=$(prop org.mpris.MediaPlayer2.Player Metadata)
printf '      Metadata: %s\n' "$MD"
yes_no "$(printf '%s' "$MD" | grep -q "'xesam:title': <'Probe Song é'>" && echo 1)" "Metadata xesam:title (UTF-8)"
yes_no "$(printf '%s' "$MD" | grep -q "'xesam:artist': <\['Probe Artist'\]>" && echo 1)" "xesam:artist is a list"
yes_no "$(printf '%s' "$MD" | grep -q "'xesam:album': <'Probe Album'>" && echo 1)" "xesam:album"
yes_no "$(printf '%s' "$MD" | grep -q "'xesam:albumArtist': <\['Probe Band'\]>" && echo 1)" "xesam:albumArtist"
yes_no "$(printf '%s' "$MD" | grep -q "'xesam:trackNumber': <4>" && echo 1)" "xesam:trackNumber"
yes_no "$(printf '%s' "$MD" | grep -q "'mpris:trackid': <objectpath '/" && echo 1)" "mpris:trackid is an object path"
ART=
i=0; while [ $i -lt 30 ]; do
    MD=$(prop org.mpris.MediaPlayer2.Player Metadata)
    ART=$(printf '%s' "$MD" | sed -n "s/.*'mpris:artUrl': <'file:\/\/\([^']*\)'>.*/\1/p")
    [ -n "$ART" ] && break; sleep 0.2; i=$((i + 1))
done
printf '      artUrl: %s\n' "$ART"
yes_no "$(case "$ART" in "$XDG_CACHE_HOME"/stained-glass/mpris/*.jpg) echo 1;; esac)" "mpris:artUrl is a .jpg in the user's cache" "$ART"
yes_no "$([ -n "$ART" ] && [ -f "$ART" ] && [ "$(head -c 2 "$ART" | od -An -tx1 | tr -d ' ')" = ffd8 ] && echo 1)" "the cover file holds the thumbnail's JPEG"
yes_no "$([ -n "$ART" ] && [ "$(stat -c %a "$(dirname "$ART")")" = 700 ] && echo 1)" "the cover folder is the user's only (0700)"
ST=$(prop org.mpris.MediaPlayer2.Player PlaybackStatus)
yes_no "$(printf '%s' "$ST" | grep -q "'Playing'" && echo 1)" "PlaybackStatus Playing" "$ST"
F=
for p in CanPlay CanPause CanGoNext CanGoPrevious CanControl; do F="$F $p=$(prop org.mpris.MediaPlayer2.Player $p | grep -o 'true\|false')"; done
printf '      flags:%s\n' "$F"
yes_no "$([ "$F" = " CanPlay=true CanPause=true CanGoNext=true CanGoPrevious=false CanControl=true" ] && echo 1)" \
    "CanPlay/CanPause/CanGoNext from IsXEnabled, CanGoPrevious off"
CS=$(prop org.mpris.MediaPlayer2.Player CanSeek)
yes_no "$(printf '%s' "$CS" | grep -q false && echo 1)" "no timeline yet: CanSeek false" "$CS"
RO=$(gdbus call --session -d "$NAME" -o /org/mpris/MediaPlayer2 -m org.freedesktop.DBus.Properties.Set org.mpris.MediaPlayer2.Player Rate '<2.0>' 2>&1)
yes_no "$(printf '%s' "$RO" | grep -qi "error" && echo 1)" "Properties.Set is refused (read-only)" "$RO"

dbus-monitor --session "type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged',path='/org/mpris/MediaPlayer2'" \
    "type='signal',interface='org.mpris.MediaPlayer2.Player',member='Seeked'" > "$T/signals" 2>/dev/null & MP=$!
sleep 0.5
call PlayPause; sleep 0.3; call Next
stage paused || { fail "the probe did not see the buttons"; cat "$T/out"; exit 1; }
i=0; while ! prop org.mpris.MediaPlayer2.Player PlaybackStatus | grep -q "'Paused'" && [ $i -lt 30 ]; do sleep 0.1; i=$((i + 1)); done
ST=$(prop org.mpris.MediaPlayer2.Player PlaybackStatus)
yes_no "$(printf '%s' "$ST" | grep -q "'Paused'" && echo 1)" "PlaybackStatus follows the program (Paused)" "$ST"
sleep 0.5
yes_no "$(grep -A12 PropertiesChanged "$T/signals" | grep -q '"Paused"' && echo 1)" "PropertiesChanged carries the new PlaybackStatus"
call PlayPause; sleep 0.3; call Previous; sleep 0.5
touch "$C/go"
# (6525) the timeline: seeking and setting the position become PlaybackPositionChangeRequested
stage timeline || { fail "the probe set no timeline"; cat "$T/out"; exit 1; }
sleep 0.5
CS=$(prop org.mpris.MediaPlayer2.Player CanSeek)
yes_no "$(printf '%s' "$CS" | grep -q true && echo 1)" "a timeline with a seekable range: CanSeek true" "$CS"
POS=$(prop org.mpris.MediaPlayer2.Player Position)
yes_no "$(printf '%s' "$POS" | grep -q "<int64 10000000>" && echo 1)" "Position is the timeline's, from its start (10 s, paused)" "$POS"
MD=$(prop org.mpris.MediaPlayer2.Player Metadata)
yes_no "$(printf '%s' "$MD" | grep -q "'mpris:length': <int64 200000000>" && echo 1)" "mpris:length is the timeline's (200 s)"
TRACK=$(printf '%s' "$MD" | sed -n "s/.*'mpris:trackid': <objectpath '\([^']*\)'>.*/\1/p")
gdbus call --session -d "$NAME" -o /org/mpris/MediaPlayer2 -m org.mpris.MediaPlayer2.Player.Seek 5000000 >/dev/null 2>&1; sleep 0.3
gdbus call --session -d "$NAME" -o /org/mpris/MediaPlayer2 -m org.mpris.MediaPlayer2.Player.SetPosition "$TRACK" 50000000 >/dev/null 2>&1; sleep 0.3
gdbus call --session -d "$NAME" -o /org/mpris/MediaPlayer2 -m org.mpris.MediaPlayer2.Player.SetPosition /org/stainedglass/mpris/stale 70000000 >/dev/null 2>&1; sleep 0.3
gdbus call --session -d "$NAME" -o /org/mpris/MediaPlayer2 -m org.mpris.MediaPlayer2.Player.Seek -- -100000000 >/dev/null 2>&1; sleep 0.3
gdbus call --session -d "$NAME" -o /org/mpris/MediaPlayer2 -m org.mpris.MediaPlayer2.Player.SetPosition "$TRACK" 190000000 >/dev/null 2>&1
stage seeked || { fail "the probe did not get the positions"; cat "$T/out"; exit 1; }
sleep 0.7
yes_no "$(grep -A2 'member=Seeked' "$T/signals" | grep -q 'int64 50000000' && echo 1)" "the program's jump to 50 s is a Seeked signal"
POS=$(prop org.mpris.MediaPlayer2.Player Position)
yes_no "$(printf '%s' "$POS" | grep -q "<int64 50000000>" && echo 1)" "and Position follows" "$POS"
touch "$C/go3"
stage off || { fail "the probe did not finish"; cat "$T/out"; exit 1; }
i=0; while [ -n "$(names)" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
yes_no "$([ -z "$(names)" ] && echo 1)" "a disabled session leaves the bus" "$(names)"
yes_no "$([ -n "$ART" ] && [ ! -e "$ART" ] && echo 1)" "its cover file is removed"
touch "$C/go2"
wait "$PP" 2>/dev/null; PP=
rm -rf "$XDG_CACHE_HOME/stained-glass"
nobus=$(env -u DBUS_SESSION_BUS_ADDRESS XDG_RUNTIME_DIR="$T/norun" timeout -s KILL 120 "$WINE" 'C:\probe.exe' nobus 2>/dev/null | tr -d '\r')
yes_no "$(printf '%s\n' "$nobus" | grep -q '^nobus=1' && echo 1)" "no session bus: the controls work as before" "$(printf '%s' "$nobus" | tail -n 1)"
yes_no "$([ ! -e "$XDG_CACHE_HOME/stained-glass/mpris" ] || [ -z "$(ls -A "$XDG_CACHE_HOME/stained-glass/mpris")" ] && echo 1)" \
    "and publish nothing (no cover written)"
out=$(tr -d '\r' < "$T/raw")
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check controls     "a window's transport controls"
check copied       "the music properties and cover from a tagged file"
check pause        "PlayPause while Playing comes back as ButtonPressed Pause"
check next         "Next comes back as ButtonPressed Next"
check other_thread "raised from a thread of the controls (not the caller's)"
check sta_thread   "a handler that is not agile is called in its single-threaded apartment (6528: Firefox)"
check play         "PlayPause while Paused comes back as Play"
check no_previous  "Previous, not enabled, is not raised"
check timeline     "UpdateTimelineProperties with a seekable range"
check seek_forward "Seek(+5 s) is PlaybackPositionChangeRequested at 15 s (in the timeline's own time)"
check set_position "SetPosition(track, 50 s) at 50 s"
check seek_clamped "Seek(-100 s) at the seekable start (5 s)"
check set_clamped  "SetPosition past the seekable end at its end (180 s)"
check stale_ignored "SetPosition for another track is ignored"
check done         "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
