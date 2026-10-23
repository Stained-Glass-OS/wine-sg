#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Media leftovers (patches/sg/6514): SystemMediaTransportControlsDisplayUpdater's
# CopyFromFileAsync reads a file's own tags (ID3v2/ID3v1, MP4, FLAC, Ogg) into the music and video
# properties and the thumbnail; MediaDevice's DefaultAudioRender/CaptureDeviceChanged are raised
# when the default device changes (test/wmedia-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/wmedia-gate.sh
# Mutants: SG_MUTANT_MC_NO_TAGS (windows.media.mediacontrol metadata.c), SG_MUTANT_MD_NO_DEVICE_EVENTS
# (windows.media.devices main.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wmedia.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wmedia-probe.c" -lole32 -luuid -lruntimeobject -luser32 -ladvapi32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 180 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check updater       "a window's transport controls and display updater"
check id3v24        "CopyFromFileAsync: ID3v2.4 (UTF-8, UTF-16, Latin-1 frames, track, genre number, cover)"
check id3v23        "ID3v2.3 frame sizes (a frame longer than 127 bytes)"
check id3v1         "ID3v1 (title, artist, album, track, genre)"
check mp4           "MP4 item list (title, artist, album artist, track, cover)"
check flac          "FLAC Vorbis comments and picture"
check ogg           "Ogg Vorbis comments"
check untagged      "no tags: the file's name is the title"
check video         "a video's title, and the playback type"
check null_file     "no file is an error"
check driver_key    "the audio driver's settings key exists"
check render_event  "DefaultAudioRenderDeviceChanged with the new device's ID and the Default role"
check render_once   "once for one change"
check capture_event "DefaultAudioCaptureDeviceChanged for the communications input"
check removed       "a removed handler is not called"
check done             "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
