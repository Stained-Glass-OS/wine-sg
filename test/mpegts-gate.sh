#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# MPEG-TS files open (patches/sg/9415).
#
# GStreamer's MPEG-TS demuxer takes the stream's sync point from the offset its
# source writes on each buffer; Wine's source pad left it unset (-1), the demuxer
# began reading at offset -1 and reached the end at once ("No program activated
# before EOS"): no .ts/.m2ts/.mts file opened in Media Foundation
# (MF_E_UNSUPPORTED_BYTESTREAM_TYPE) or DirectShow (VFW_E_CANNOT_RENDER).
#
# The probe (test/mpegts-probe.c) writes a 0.43 s MPEG-TS (one MPEG-1 layer II
# stream) and opens it: source resolver, source reader to PCM to the end of the
# stream, DirectShow RenderFile.
#   WINE=/opt/wine-sg/bin/wine test/mpegts-gate.sh
# Mutant: SG_MUTANT_PULL_NO_OFFSET (winegstreamer wg_parser.c)
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

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-mpegts.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;${WINEDLLOVERRIDES:-}"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/mpegts-probe.c" -lole32 -luuid -lstrmiids -lmfplat -lmfreadwrite -lmfuuid \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
# to a file: services started with the prefix keep a pipe open
timeout -s KILL 120 "$WINE" 'C:\probe.exe' >"$T/out" 2>/dev/null
out=$(tr -d '\r' < "$T/out")
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check write  "the probe wrote its MPEG-TS"
check source "the source resolver opens it"
check reader "a source reader opens it"
check pcm    "its audio decodes to PCM"
check read   "the reader reads it to the end"
check render "DirectShow renders it"
exit $RC
