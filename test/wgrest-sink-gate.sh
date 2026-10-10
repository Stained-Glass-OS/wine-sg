#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winegstreamer's media_sink.c stubs (patches/sg/2921), on Xvfb: the probe
# test/wgrest-sink-probe.c makes an MPEG4 media sink and checks
# GetCharacteristics, Set/GetPresentationClock (registration with the clock,
# replacing, shutdown), OnClockSetRate, RemoveStreamSink and re-adding a
# stream, the stream sink IMFMediaTypeHandler (major type, type count / by
# index, IsMediaTypeSupported, SetCurrentMediaType), PlaceMarker and Flush
# (events, order, context) and the results after Shutdown. No FIXME may be
# logged by those functions.
#
#   WINE=/opt/wine-sg/bin/wine test/wgrest-sink-gate.sh
# Mutants (winegstreamer, -DSG_MUTANT_x): CHARFLAGS (fixed streams flag),
# CLOCKREG (the sink is never registered with the clock), MARKERCTX (marker
# event without the context), MARKERDROP (markers queued before a Flush are
# dropped), TYPECOUNT (type count 0), RMKEEP (RemoveStreamSink keeps the stream).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wgrestsink.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+mfplat WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -Wno-format -o "$T/wgrest-sink-probe.exe" "$HERE/wgrest-sink-probe.c" -lole32 -luuid -luser32 -lmfplat -lmf -lmfuuid -lpropsys \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wgrest-sink-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" wgrest-sink-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:mfplat:(stream_sink_|media_sink_)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
