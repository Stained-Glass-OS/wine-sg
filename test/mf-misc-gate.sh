#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mf's audio renderer policy and clock rate (patches/sg/2834), on Xvfb:
# test/mf-misc-probe.c uses IMFAudioPolicy (grouping parameter, display name,
# icon path) and IMFClockStateSink::OnClockSetRate of MFCreateAudioRenderer's
# sink, before and after Shutdown. Needs an audio render device (it passes with
# a note without one).
#
#   WINE=/opt/wine-sg/bin/wine test/mf-misc-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (mf, -DSG_MUTANT_x): SAR_POLICY_NOSTORE, SAR_SHUTDOWN, SAR_RATE, MF_ENUM_EMPTY.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mfmisc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/mf-misc-probe.exe" "$HERE/mf-misc-probe.c" -lmf -lmfplat -lmfuuid -lole32 -luuid -lstrmiids -lm \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/mf-misc-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" mf-misc-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:[a-z]+:(class_factory_LockServer|classfactory_LockServer|video_stream_get_service|audio_renderer_policy_|audio_renderer_clock_sink_OnClockSetRate)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
