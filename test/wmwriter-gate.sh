#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Windows Media writer writes ASF files (patches/sg/6517; BeginWriting was E_NOTIMPL): a
# profile's PCM audio and RGB24 video streams through WriteSample and WriteStreamSample, read back
# as the ASF specification lays the file out (test/wmwriter-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/wmwriter-gate.sh
# Mutants: SG_MUTANT_WMVW_NO_PAYLOAD, SG_MUTANT_WMVW_NO_DESCRIPTIONS (wmvcore asfmux.c).
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
T=$(mktemp -d /var/tmp/sg-wmwriter.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wmwriter-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 180 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check profile             "a profile of a PCM audio and an RGB24 video stream"
check inputs              "inputs in the streams' own formats"
check begin               "BeginWriting starts"
check write_sample        "WriteSample takes the audio and the video"
check write_stream_sample "WriteStreamSample takes a stream sample"
check statistics          "the statistics count the samples and bytes"
check end                 "EndWriting finishes the file"
check sink                "a sink gets the header, the packets and the end"
check end_twice           "EndWriting again fails"
check header              "the header object: file and stream properties, header extension"
check streams             "the streams' formats as the profile gave them"
check title               "the Title attribute in the content description"
check durations           "the play duration and the preroll"
check packets             "the data object's packets, counted in the file properties"
check audio_data          "every audio sample comes back from the packets, byte for byte"
check video_data          "every video frame too, with its clean point"
check times               "presentation times in milliseconds after the preroll"
check index               "a simple index for the video stream"
check file_size           "the file properties carry the file's size"
check done             "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
