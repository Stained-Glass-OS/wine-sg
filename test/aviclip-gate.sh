#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# AVI files on the clipboard (patches/sg/6511; AVIPutFileOnClipboard, AVIGetFromClipboard,
# AVIMakeStreamFromClipboard and AVIClearClipboard were stubs): this process gets its file back,
# every program gets the first frame (CF_DIB) and the sound (CF_WAVE), another process gets a file
# of them; DIBs and WAVEs become editable streams (test/aviclip-probe.c), under Xvfb.
#   WINE=/opt/wine-sg/bin/wine test/aviclip-gate.sh
# Mutants: SG_MUTANT_AVICLIP_NO_RENDER (avifil32 clipboard.c), SG_MUTANT_AVI_AUDIO_LENGTH_BLOCKS
# (avifil32 avifile.c: the writer counted a sound stream in blocks).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-aviclip.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/aviclip-probe.c" -lavifil32 -lole32 -luuid -luser32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check written      "the probe writes its AVI (two frames, half a second of sound)"
check orig_audio   "the AVI writer counts a sound stream in samples (one write of 4000 samples is 4000)"
check put          "AVIPutFileOnClipboard puts it on the clipboard"
check same_file    "AVIGetFromClipboard gives this process its file"
check clip_dib     "every program gets the first frame as CF_DIB"
check clip_wave    "and the sound as CF_WAVE"
check child_get    "another process gets a file from the clipboard"
check child_video  "with the first frame as its video"
check child_audio  "and the whole sound as its audio"
check dib_stream   "AVIMakeStreamFromClipboard: a DIB is an editable one-frame video stream"
check wave_stream  "a WAVE is an audio stream of its samples"
check other_format "another format is refused"
check cleared      "AVIClearClipboard takes the file off the clipboard"
check done         "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
