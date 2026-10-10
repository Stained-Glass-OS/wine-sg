#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winegstreamer's color converter DMO (patches/sg/2881), on Xvfb:
# test/wgdmo-probe.c colorconv drives the IMediaObject interface (stream count /
# info, type enumeration, SetType flag and error rules, current types, size
# info, latency, Lock from a second thread, input status, Flush, Discontinuity,
# ProcessInput / ProcessOutput with real audio), the IPropertyBag and
# IPropertyStore objects, IWMResamplerProps and the IMFTransform status / message
# methods. The run's log must not hold a FIXME from those functions.
#
#   WINE=/opt/wine-sg/bin/wine test/wgdmo-colorconv-gate.sh
# Mutants (winegstreamer, -DSG_MUTANT_x): CC_NOFLUSH (the MFT FLUSH message does
# nothing), CC_STATUSID (GetInputStatus ignores the stream id), DMO_OUTCOMPLETE
# (output types do not take the input's frame size); the shared DMO_* mutants
# listed in test/wgdmo-resampler-gate.sh are caught here too.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
COMPONENT=colorconv
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wgdmo.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+mfplat,fixme+wmadec WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -Wno-format -o "$T/wgdmo-probe.exe" "$HERE/wgdmo-probe.c" -lmfplat -lmfuuid -lole32 -loleaut32 -luuid -lpropsys -lstrmiids \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wgdmo-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" wgdmo-probe.exe $COMPONENT 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -qx 'RESULT: SKIP' "$T/probe.out"; then echo "SKIP: $COMPONENT cannot be created here"; exit 77; fi
if grep -E 'fixme:(mfplat|wmadec):(media_object_|transform_|property_|resampler_props_|get_available_media_type|dmo_)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
