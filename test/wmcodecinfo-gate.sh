#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The profile manager's codec information (patches/sg/9105; IWMCodecInfo was E_NOINTERFACE, so
# fre:ac's WMA encoder had no codec to configure): the codecs as fre:ac walks them, and every audio
# format offered is encoded and read back (test/wmcodecinfo-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/wmcodecinfo-gate.sh
# Needs gstreamer1.0-libav (avenc_wmav2) and gstreamer1.0-plugins-ugly (asfdemux, for the reader).
# Mutants: SG_MUTANT_CODECINFO_NO_DATA (wmvcore codecinfo.c)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for so in libgstlibav.so libgstasf.so; do
    ls /usr/lib/*/gstreamer-1.0/$so >/dev/null 2>&1 || { echo "SKIP: no $so (gstreamer1.0-libav, -plugins-ugly)"; exit 77; }
done
T=$(mktemp -d /var/tmp/sg-wmcodecinfo.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wmcodecinfo-probe.c" -lole32 -luuid -lm || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 300 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check interface          "the profile manager has IWMCodecInfo3"
check same_object        "and it is the profile manager"
check audio_codec        "a standard Windows Media Audio codec (fre:ac's default)"
check vbr_consistent     "what _ISVBRSUPPORTED says is what the VBR list holds"
check cbr_formats        "one-pass CBR formats"
check cd_format          "44.1 kHz stereo 128 kbps among them"
check all_encode         "every format offered encodes and reads back"
check video_codec        "a Windows Media Video codec with a format"
check bad_index          "a codec index out of range is E_INVALIDARG"
check done               "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
