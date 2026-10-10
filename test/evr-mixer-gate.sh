#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# evr's default mixer (patches/sg/2890), on Xvfb: test/evr-mixer-probe.c uses
# IMFVideoProcessor (mode, ProcAmp and filtering), IMFVideoMixerBitmap,
# IMFVideoPositionMapper, IMFQualityAdvise, IMFClockStateSink and substream
# SetInputType of MFCreateVideoMixer's mixer, with and without a D3D9 device.
#
#   WINE=/opt/wine-sg/bin/wine test/evr-mixer-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (evr, -DSG_MUTANT_x): PROCAMP_NORANGE, SUBSTREAM_CLEARS, BITMAP_NOINIT,
# MAPPER_NOSCALE, QA_ACCEPTALL, MODE_NOSTORE, FILTER_NOSTORE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-evrmixer.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/evr-mixer-probe.exe" "$HERE/evr-mixer-probe.c" -I"$HERE" -lmfplat -lmfuuid -lole32 -luuid -lstrmiids -ldxguid -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/evr-mixer-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" evr-mixer-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:evr:video_mixer_' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed mixer function logged a FIXME"
    exit 1
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
