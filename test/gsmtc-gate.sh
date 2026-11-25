#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The desktop's media sessions through Windows.Media.Control (patches/sg/9862).
# GlobalSystemMediaTransportControlsSessionManager is how launchers, widgets and "now playing" tools
# see and control what plays (Wox 2.5's media plugin asked for it: "Failed to find library").
# The sessions are the MPRIS players on the user's session bus -- Linux programs' and Windows
# programs' transport controls alike. Here a Windows player (test/gsmtc-probe.c player) publishes
# its transport controls; another program (probe manager) must find it among the sessions as
# probe.exe, the current one, with its title, artist, status Playing and Play/Pause/Next enabled
# (Previous not); TryPauseAsync must reach the player (its ButtonPressed Pause), raise
# PlaybackInfoChanged and leave the status Paused. Its own session bus (dbus-run-session), X server.
#   WINE=/opt/wine-sg/bin/wine test/gsmtc-gate.sh
# Mutant (dlls/windows.media.mediacontrol/gsmtc.c): SG_MUTANT_GSMTC_NO_SESSIONS.
set -u
if [ -z "${SG_GSMTC_BUS:-}" ]; then
    command -v dbus-run-session >/dev/null || { echo "SKIP: dbus-run-session missing"; exit 77; }
    unset DBUS_SESSION_BUS_ADDRESS
    SG_GSMTC_BUS=1 exec dbus-run-session -- sh "$0" "$@"
fi
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=; PP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-gsmtc.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; for p in $XP $PP; do kill "$p" 2>/dev/null; done; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/gsmtc-probe.c" -lole32 -luuid -lruntimeobject -luser32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG="${SG_GSMTC_DEBUG:--all}" WINESERVER XDG_CACHE_HOME="$T/cache"
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"

timeout -s KILL 180 "$WINE" 'C:\probe.exe' player > "$T/player" 2>/dev/null & PP=$!
i=0; while ! tr -d '\r' < "$T/player" | grep -q "^stage=ready" && [ $i -lt 600 ]; do sleep 0.1; i=$((i + 1)); done
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' manager 2>/dev/null | tr -d '\r')
touch "$WINEPREFIX/drive_c/stop"
wait $PP 2>/dev/null; PP=
player=$(tr -d '\r' < "$T/player")
printf '%s\n' "$out" | sed 's/^/      manager: /'
printf '%s\n' "$player" | sed 's/^/      player: /'
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p" | head -1; }
printf '%s\n' "$player" | grep -q '^stage=ready' || fail "the player did not publish its transport controls"
printf '%s\n' "$out" | grep -qx END || fail "the manager probe did not finish"
[ "$(v factory)" = 0 ] && pass "GlobalSystemMediaTransportControlsSessionManager is a class" || fail "factory: '$(v factory)'"
case "$(v request)" in "0 manager=1") pass "RequestAsync gives a manager" ;; *) fail "RequestAsync: '$(v request)'" ;; esac
case "$(v sessions)" in *found=1) pass "the player is among the sessions ($(v sessions))" ;; *) fail "sessions: '$(v sessions)'" ;; esac
[ "$(v aumid)" = probe.exe ] && pass "its SourceAppUserModelId is probe.exe" || fail "aumid: '$(v aumid)'"
[ "$(v current)" = 1 ] && pass "it is the current session" || fail "current: '$(v current)'"
[ "$(v title)" = "Gate Song" ] && [ "$(v artist)" = "Gate Artist" ] && pass "title and artist" \
    || fail "media properties: '$(v title)' / '$(v artist)' $(v props)"
[ "$(v status)" = "4 play=1 pause=1 next=1 previous=0" ] && pass "Playing; Play, Pause and Next enabled, Previous not" \
    || fail "playback info: '$(v status)'"
case "$(v pause_result)" in "1 hr=0") pass "TryPauseAsync: true" ;; *) fail "TryPauseAsync: '$(v pause_result)'" ;; esac
printf '%s\n' "$player" | grep -qx 'pressed=1' && pass "the player got ButtonPressed Pause" || fail "the player got no Pause"
[ "$(v event)" = 1 ] && pass "PlaybackInfoChanged was raised" || fail "no PlaybackInfoChanged"
[ "$(v status_after)" = 5 ] && pass "the status is Paused" || fail "status after: '$(v status_after)'"
[ $RC = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
