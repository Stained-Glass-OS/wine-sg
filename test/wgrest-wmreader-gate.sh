#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winegstreamer's wm_reader.c remaining stubs (patches/sg/2920), on Xvfb: the
# probe test/wgrest-wmreader-probe.c opens test/wgst-wmreader-test.wmv in a sync
# reader and checks IWMHeaderInfo (attribute count / index / name / indices,
# markers, scripts, codec info, read-only editing calls), IWMLanguageList,
# IWMPacketSize, IWMReaderTimecode, IWMReaderPlaylistBurn, the 16 output
# settings on an audio and a video output, GetMaxOutputSampleSize and
# SetRangeByFrame / ByFrameEx / ByTimecode. The log must not hold a FIXME
# from those functions.
#
#   WINE=/opt/wine-sg/bin/wine test/wgrest-wmreader-gate.sh
# Mutants (winegstreamer, -DSG_MUTANT_x): HDRCOUNT (file attribute count 0),
# SETKEEP (SetOutputSetting stores nothing), FRAMESTART (range by frame starts
# at frame 1 less), OUTSIZE (max output sample size 0), CODECTYPE (codec info
# type swapped), LANGCOUNT.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wgrestwmr.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+wmvcore WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -Wno-format -o "$T/wgrest-wmreader-probe.exe" "$HERE/wgrest-wmreader-probe.c" -lole32 -luuid -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wgrest-wmreader-probe.exe" "$WINEPREFIX/drive_c/"
cp "$HERE/wgst-wmreader-test.wmv" "$WINEPREFIX/drive_c/test.wmv"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" wgrest-wmreader-probe.exe 'C:\\test.wmv' 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:wmvcore:(header_info_|language_list_|packet_size_|timecode_|playlist_(Get|Cancel|End)|reader_(GetMaxOutput|GetOutputSetting|SetOutputSetting|SetRangeBy)|unknown_inner_Query)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
