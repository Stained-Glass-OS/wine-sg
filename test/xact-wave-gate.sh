#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# xactengine's PrepareInMemoryWave / PrepareStreamingWave (patches/sg/2972):
# test/xact-wave-probe.c builds waves from generated PCM data (no sound bank) and
# checks their format, duration, streaming flag and state; the run's log may not
# hold a FIXME from xact3. The cue channel map / volume / output voice methods
# need a sound bank (cue objects) and are covered only by the conformance test
# build; SKIP (77) when the engine cannot initialise (no audio output).
#
#   WINE=/opt/wine-sg/bin/wine test/xact-wave-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (xactengine3_7, -DSG_MUTANT_x): WAVE_INMEM, WAVE_STREAM (unimplemented, E_NOTIMPL),
# WAVE_PTR, WAVE_SPTR (output pointer unchecked), WAVE_ARGS (NULL wave data unchecked).
# The bundled FAudio has no FACTWave support yet (Prepare*Wave return no wave): the probe then
# expects E_FAIL, and checks the wave values only once FAudio returns a wave.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xactwave.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+xaudio2,fixme+xact3 WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/xact-wave-probe.c" -DXACT3_VER=0x0307 -w -I "$HERE/xact-inc" -ldxguid -lole32 -luuid -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
( cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 env DISPLAY= "$WINE" probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out" )
cat "$T/probe.out"
if grep -E 'fixme:xact3:' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -q '^SKIP' "$T/probe.out" && exit 77
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
