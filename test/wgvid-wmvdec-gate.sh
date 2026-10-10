#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winegstreamer's WMV video decoder object (patches/sg/2910), on Xvfb:
# test/wgvid-probe.c wmvdec drives the IMediaObject interface of the WMV decoder (stream
# info, current types, size info, input type validation, GetOutputType rules, latency,
# Lock from a second thread, streaming resources, ProcessInput / ProcessOutput with a
# real WMV frame, S_FALSE without data, the discard flag), the IPropertyBag and
# IPropertyStore objects, and the transform attribute objects / optional MFT methods.
# The run's log must not hold a FIXME from those functions.
#
#   WINE=/opt/wine-sg/bin/wine test/wgvid-wmvdec-gate.sh
# Mutants (winegstreamer, -DSG_MUTANT_x): DMO_LOCK DMO_LATENCY DMO_SFALSE DMO_STREAMIDX
# DMO_BAG DMO_STORE DMO_DISCARD (shared helpers, dmo_transform.c) and WMVDEC_FLAGS (stream
# flags 0), WMVDEC_CURRENT (no current type), WMVDEC_SIZE (input size 0), WMVDEC_FORMAT
# (the bitmap header is not checked).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
COMPONENT=wmvdec
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wgvid.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+mfplat WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -Wno-format -o "$T/wgvid-probe.exe" "$HERE/wgvid-probe.c" -lmfplat -lmfuuid -lole32 -loleaut32 -luuid -lpropsys -lstrmiids \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wgvid-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" wgvid-probe.exe $COMPONENT 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -qx 'RESULT: SKIP' "$T/probe.out"; then echo "SKIP: $COMPONENT cannot be created here"; exit 77; fi
if grep -E 'fixme:mfplat:(media_object_|transform_|property_|codec_api_|video_processor_|dmo_)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
