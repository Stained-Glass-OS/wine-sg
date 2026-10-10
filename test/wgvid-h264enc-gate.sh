#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winegstreamer's H.264 encoder object (patches/sg/2911), on Xvfb:
# test/wgvid-probe.c h264enc drives the ICodecAPI interface of the H.264 encoder (support,
# modifiability, range / values / default, get and set with type and range validation, the
# defaults that follow the output type, SetAllDefaults, the notification lists, the settings
# stream, no events) and the IMFTransform input status and messages.
# The run's log must not hold a FIXME from those functions.
#
#   WINE=/opt/wine-sg/bin/wine test/wgvid-h264enc-gate.sh
# Mutants (winegstreamer, -DSG_MUTANT_x): CODEC_RANGE (no range check on SetValue), CODEC_SET (SetValue stores
# nothing), CODEC_KIND (enumerated parameters report a range), ENC_DERIVED (mean bit rate and
# buffer size ignore the output type), ENC_NOTIFY (SetValueWithNotify lists nothing),
# ENC_ALLSET (a settings stream is not validated), ENC_STATUS (input status never accepts),
# ENC_DRAIN (the DRAIN message succeeds without types).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
COMPONENT=h264enc
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
