#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winmm's small stubs (patches/sg/2832), on Xvfb: test/winmm-misc-probe.c checks
# mixerGetLineInfo with TARGETTYPE, midiOutCache(Drum)Patches, joyConfigChanged
# and mmioWrite growing a memory file. The mixer part needs an audio device
# and the MIDI part a MIDI output; without one the probe says so and checks
# what it can.
#
#   WINE=/opt/wine-sg/bin/wine test/winmm-misc-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (winmm, -DSG_MUTANT_x): WINMM_TARGETTYPE, WINMM_MIDI_CACHE_HANDLE,
# WINMM_MMIO_GROW, WINMM_ELEMENT_ID.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-winmmmisc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/winmm-misc-probe.exe" "$HERE/winmm-misc-probe.c" -lwinmm \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/winmm-misc-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" winmm-misc-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
