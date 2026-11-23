#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mf: MFCreateAggregateSource (patches/sg/8901; test/aggsource-probe.c). Qt Multimedia joins a camera and a
# microphone with it for capture sessions with audio (Kdenlive, Anki, calibre, MusicBrainz Picard,
# MKVToolNix, Wireshark import it); Wine had a stub. Two WAV sources, read through a source reader.
#   WINE=/opt/wine-sg/bin/wine test/aggsource-gate.sh
# Mutant: SG_MUTANT_AGG_SAME_IDS (dlls/mf/aggregate.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-aggsource.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
LIBS="-lmf -lmfplat -lmfreadwrite -lmfuuid -lole32 -luuid"
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/aggsource-probe.c" $LIBS 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
command -v i686-w64-mingw32-gcc >/dev/null &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O2 -o "$T/probe32.exe" "$HERE/aggsource-probe.c" $LIBS 2>>"$T/cc.log"
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
for p in probe probe32; do
    [ -f "$T/$p.exe" ] || continue
    echo "== $p"
    timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null > "$T/$p.raw"
    tr -d '\r' < "$T/$p.raw" | tee "$T/$p.out"
    grep -q '^SKIP' "$T/$p.out" && { echo "SKIP: no WAV media source"; exit 77; }
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
