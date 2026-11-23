#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mf.dll's WMContainer (ASF) objects and the ASF media sink (patches/sg/9401, 9402; test/asfmf-probe.c).
# They were spec stubs: programs writing or reading ASF (WMA/WMV) files with Media Foundation's ASF
# components (MFCreateASFProfile/ContentInfo/Multiplexer/Splitter, as in Microsoft's tutorials), the
# ASF file sink (MFCreateASFMediaSink(Activate)) and sink writers to .wma/.wmv/.asf got nothing. The
# probe builds a profile, multiplexes samples into packets, writes GenerateHeader's output and the
# packets, parses them back (ParseHeader, splitter) byte for byte, writes through a sink writer, and
# has GStreamer's own ASF demuxer (a source reader) read every file back.
#   WINE=/opt/wine-sg/bin/wine test/asfmf-gate.sh
# Mutants: SG_MUTANT_ASF_NO_DATA_OBJECT, SG_MUTANT_ASF_NO_PAYLOAD, SG_MUTANT_ASF_NO_DURATION (dlls/mf/asf.c),
# SG_MUTANT_ASFSINK_NO_FINAL_HEADER (dlls/mf/asf_sink.c), SG_MUTANT_WRITER_NO_ASF (dlls/mfreadwrite/writer.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-asfmf.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all,err+winediag} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
LIBS="-lmf -lmfplat -lmfreadwrite -lmfuuid -lole32 -luuid"
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/asfmf-probe.c" $LIBS 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
command -v i686-w64-mingw32-gcc >/dev/null &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O2 -o "$T/probe32.exe" "$HERE/asfmf-probe.c" $LIBS 2>>"$T/cc.log"
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
for p in probe probe32; do
    [ -f "$T/$p.exe" ] || continue
    echo "== $p"
    timeout -s KILL 180 "$WINE" "$T/$p.exe" 2>"$T/$p.err" </dev/null > "$T/$p.raw"
    tr -d '\r' < "$T/$p.raw" | tee "$T/$p.out"
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
