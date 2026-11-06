#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# IAudioEndpointVolume (patches/sg/0777): the default render device's volume
# has its channels, master and per-channel volume in dB and scalar, mute,
# steps and ranges, one per device in the process (set through one object,
# read through another), and its callbacks hear of changes. MeediOS's
# MasterVolume plugin failed at GetChannelCount (E_NOTIMPL).
# Skips (77) where Wine finds no audio endpoint.
#
#   WINE=/opt/wine-sg/bin/wine test/endpointvolume-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-aev.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/endpointvolume-probe.exe" "$HERE/endpointvolume-probe.c" -lole32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cp "$T/endpointvolume-probe.exe" "$WINEPREFIX/drive_c/"
timeout -s KILL 60 "$WINE" 'C:\endpointvolume-probe.exe' 2>/dev/null | tr -d '\r' > "$T/probe.out"
grep -qx SKIP "$T/probe.out" && { echo "SKIP: no audio endpoint here"; exit 77; }
for c in channels scalar db notify channel channel-range steps range mute; do
    l=$(grep "^$c " "$T/probe.out")
    case "$l" in "$c OK") echo "PASS  $c" ;; *) echo "FAIL  ${l:-$c: no answer}"; RC=1 ;; esac
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
