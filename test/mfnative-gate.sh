#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Media Foundation's sources and audio decoders as Windows has them (test/mfnative-probe.c):
#  - the MPEG-4, ASF and MP3 sources' audio is as it is in the file -- the source reader's native type is
#    AAC / WMAudioV8 / MP3, not decoded PCM (patches/sg/8904) -- and PCM asked of a source reader or of a
#    Media Session's topology is decoded by the decoder MFT they add;
#  - the MP3 decoder is an MFT as well as a DMO, "MP3 Decoder MFT", found by MFTEnumEx (8903);
#  - the WMA decoder MFT decodes samples fed by hand: whole, cut into blocks, and with the type the ASF
#    header gives (a probe of 2026-11-23 had decoded garbage; not seen once the encoder wrote real codec
#    data, 9407 -- kept as a check);
#  - MFTEnumEx hands out a registered transform's name (MFT_FRIENDLY_NAME_Attribute, 8903).
# The fixtures come from Wine's own encoders (the transcode API, 9405) and hold a 1 kHz tone.
#   WINE=/opt/wine-sg/bin/wine test/mfnative-gate.sh
# Mutants: SG_MUTANT_SOURCE_DECODES_AUDIO (winegstreamer/wg_parser.c), SG_MUTANT_MP3MFT_NONE and
# SG_MUTANT_MP3MFT_TAKES_ALL (mp3dmod/mp3dmod.c), SG_MUTANT_MFT_NO_NAME (mfplat/main.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-mfnative.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all,err+winediag} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
LIBS="-lmf -lmfplat -lmfreadwrite -lmfuuid -lole32 -luuid -lstrmiids -ldmoguids"
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/mfnative-probe.c" $LIBS 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 400 "$WINE" "$T/probe.exe" 2>"$T/probe.err" </dev/null > "$T/probe.raw"
tr -d '\r' < "$T/probe.raw" > "$T/probe.out"
cat "$T/probe.out"
[ -n "${GATE_DEBUG:-}" ] && tail -40 "$T/probe.err"
if grep -q "GStreamer doesn't support AAC encoding" "$T/probe.err"; then
    echo "SKIP: GStreamer has no AAC encoder here (gstreamer1.0-libav)"; exit 77
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"
exit 1
