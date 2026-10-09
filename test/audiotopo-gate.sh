#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Device topology and audio session events (patches/sg/1666): on the
# default render endpoint, test/audiotopo-probe.c walks the endpoint's
# topology to its adapter's jack, mute, volume and stream connector, uses
# IAudioVolumeLevel/IAudioMute and their change callbacks, reads the jack
# (IKsJackDescription), and hears IAudioSessionNotification and
# IAudioSessionEvents (volume, channel volume, name, state).
# These were stubs. Skips (77) where Wine finds no audio endpoint.
#
#   WINE=/opt/wine-sg/bin/wine test/audiotopo-gate.sh
# Mutants: SG_MUTANT_NO_CONTROL_CALLBACKS (mmdevapi/topology.c),
# SG_MUTANT_NO_SESSION_EVENTS (mmdevapi/session.c and
# mmdevapi/audiosessionmanager.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-audiotopo.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/audiotopo-probe.exe" "$HERE/audiotopo-probe.c" -lole32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cp "$T/audiotopo-probe.exe" "$WINEPREFIX/drive_c/"
timeout -s KILL 120 "$WINE" 'C:\audiotopo-probe.exe' 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
grep -qx SKIP "$T/probe.out" && { echo "SKIP: no audio endpoint here"; exit 77; }
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
