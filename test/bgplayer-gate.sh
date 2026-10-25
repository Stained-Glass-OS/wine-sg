#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A desktop player's transport controls through BackgroundMediaPlayer (patches/sg/6536): foobar2000
# asks Windows.Media.Playback.BackgroundMediaPlayer for its Current player and that player's
# SystemMediaTransportControls; with Windows.Media.Playback missing it got none, and was not on the
# desktop's media controls (MPRIS) or reachable by the media keys. Now Current is one player, its
# controls one object, published on the bus like a window's, and a bus PlayPause comes back as
# ButtonPressed Pause. A window's GetForWindow gives the same controls each time
# (test/bgplayer-probe.c). Its own session bus (dbus-run-session) and X server.
#   WINE=/opt/wine-sg/bin/wine test/bgplayer-gate.sh
# Mutants: SG_MUTANT_NO_PLAYBACK_REGISTRATION (tools/widl/register.c: classes declared in classes.idl
# not registered), SG_MUTANT_SMTC_NEW_PER_CALL (dlls/windows.media.mediacontrol/main.c).
set -u
if [ -z "${SG_BGPLAYER_BUS:-}" ]; then
    command -v dbus-run-session >/dev/null || { echo "SKIP: dbus-run-session missing"; exit 77; }
    unset DBUS_SESSION_BUS_ADDRESS
    SG_BGPLAYER_BUS=1 exec dbus-run-session -- sh "$0" "$@"
fi
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=; PP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
yes_no() { if [ "$1" = 1 ]; then pass "$2"; else fail "$2${3:+ ($3)}"; fi; }
for t in Xvfb gdbus "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-bgplayer.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; for p in $XP $PP; do kill "$p" 2>/dev/null; done; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/bgplayer-probe.c" -lole32 -luuid -lruntimeobject -luser32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG="${SG_BGPLAYER_DEBUG:--all}" WINESERVER XDG_CACHE_HOME="$T/cache"
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"

names() { gdbus call --session -d org.freedesktop.DBus -o /org/freedesktop/DBus -m org.freedesktop.DBus.ListNames 2>/dev/null |
          grep -o "org\.mpris\.MediaPlayer2\.[A-Za-z0-9_.]*" ; }
timeout -s KILL 120 "$WINE" 'C:\probe.exe' > "$T/raw" 2>/dev/null & PP=$!
i=0; while ! tr -d '\r' < "$T/raw" | grep -q "^stage=ready" && [ $i -lt 300 ]; do sleep 0.1; i=$((i + 1)); done
i=0; while [ -z "$(names)" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
NAME=$(names | head -n 1)
STATUS=
if [ -n "$NAME" ]; then
    STATUS=$(gdbus call --session -d "$NAME" -o /org/mpris/MediaPlayer2 -m org.freedesktop.DBus.Properties.Get org.mpris.MediaPlayer2.Player PlaybackStatus 2>&1)
    gdbus call --session -d "$NAME" -o /org/mpris/MediaPlayer2 -m org.mpris.MediaPlayer2.Player.PlayPause >/dev/null 2>&1
fi
wait $PP 2>/dev/null; PP=
out=$(tr -d '\r' < "$T/raw")
printf '%s\n' "$out" | sed 's/^/      /'
printf '      bus name: %s; status %s\n' "$NAME" "$STATUS"
check() {
    if printf '%s\n' "$out" | grep -Eq "^(.* )?$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" || echo none))"; fi
}
check window_same    "a window's GetForWindow gives the same controls each time"
check statics        "Windows.Media.Playback.BackgroundMediaPlayer is a registered class"
check same_current   "BackgroundMediaPlayer.Current is one player"
check same_controls  "its SystemMediaTransportControls are one object"
yes_no "$(printf '%s' "$NAME" | grep -q '^org\.mpris\.MediaPlayer2\.probe\.instance' && echo 1)" "enabled, they are an MPRIS player on the session bus" "$NAME"
yes_no "$(printf '%s' "$STATUS" | grep -q "'Playing'" && echo 1)" "PlaybackStatus Playing" "$STATUS"
check pause          "a bus PlayPause comes back as ButtonPressed Pause"
check done           "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
