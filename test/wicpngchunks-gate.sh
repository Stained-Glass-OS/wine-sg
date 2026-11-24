#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# windowscodecs: the PNG bKGD, sRGB, iCCP and iTXt chunks (patches/sg/9350) have readers and writers of
# their own, as on Windows: /sRGB/RenderingIntent, /bKGD/BackgroundColor, /iCCP/ProfileName and the inflated
# ProfileData, /iTXt/Keyword, LanguageTag, TranslatedKeyword, TextEntry (compressed too). They were read by
# the Unknown reader, and their writers -- with a header in the PNG container -- did not exist: Paint.NET
# 5.2 could not open any PNG with an sRGB or bKGD chunk (WICMetadataWriterInfo.GetHeader ->
# ImagingComponentNotFoundException). Also: the writer made from each reader writes the chunk back byte for
# byte, and the PNG encoder writes sRGB, bKGD and iTXt blocks it is given.
#   WINE=/opt/wine-sg/bin/wine test/wicpngchunks-gate.sh
# Mutant: SG_MUTANT_WIC_NO_NEW_CHUNKS (dlls/windowscodecs/regsvr.c: the handlers not registered).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${WICPNGCHUNKS_DPY:-385}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-wicpngchunks.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/wicpngchunks-probe.c" -lole32 -luuid -lwindowscodecs 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

rc=0
cd "$T" && timeout -s KILL 60 "$WINE" "$T/probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
