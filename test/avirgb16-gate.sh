#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# 16-bit uncompressed AVI video plays (patches/sg/9413).
#
# GStreamer's AVI demuxer has no caps for a 16-bit BI_RGB or BI_BITFIELDS stream
# (video/x-avi-unknown, no size), decodebin found no decoder for it and the AVI
# splitter failed the whole file: RenderFile 0x80040218. As Windows' splitter does,
# such a stream now goes out as it is in the file with the media type of its own
# stream header: RGB555 for BI_RGB, RGB565 by the BI_BITFIELDS masks, a
# VIDEOINFOHEADER carrying the header (and masks), the frame time from strh.
#
# The probe (test/avirgb16-probe.c) writes an RGB555 and an RGB565 AVI of a
# known pattern and reads each through File Source -> AVI Splitter -> Sample
# Grabber -> Null Renderer: the splitter's type and header, every frame's bytes
# and times; then RenderFile on each.
#   WINE=/opt/wine-sg/bin/wine test/avirgb16-gate.sh
# Mutants: SG_MUTANT_AVI_UNKNOWN_DECODED (winegstreamer wg_parser.c),
#          SG_MUTANT_AVI_NO_STREAM_HEADERS (winegstreamer quartz_parser.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sg_gate_display || exit $?

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-avirgb16.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;${WINEDLLOVERRIDES:-}"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/avirgb16-probe.c" -lole32 -loleaut32 -luuid -lstrmiids \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 180 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check write "the probe wrote its AVIs"
for f in rgb555 rgb565; do
    check ${f}_splitter "$f: the AVI splitter takes the file"
    check ${f}_type     "$f: its pin's type is the stream's subtype with a VIDEOINFOHEADER"
    check ${f}_header   "$f: the header is the stream's (size, depth, compression, masks, frame time)"
    check ${f}_connect  "$f: a Sample Grabber for that subtype connects"
    check ${f}_frames   "$f: all frames arrive and the graph completes"
    check ${f}_bytes    "$f: the frames are the file's bytes"
    check ${f}_times    "$f: the frames' times follow the stream's rate"
    check ${f}_render   "$f: RenderFile renders the file"
done
exit $RC
