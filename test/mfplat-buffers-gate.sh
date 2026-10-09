#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mfplat's buffer and conversion stubs (patches/sg/2830), on Xvfb:
# test/mfplat-buffers-probe.c calls MFCreateMediaBufferWrapper,
# MFCalculateBitmapImageSize, MFConvertTo/FromFP16Array, the legacy buffer's
# offset, IMF2DBuffer2::Copy2DTo and the 2D buffer's IMFGetService, and checks
# the results (all 65536 half values round-trip; table of conversions).
#
#   WINE=/opt/wine-sg/bin/wine test/mfplat-buffers-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (mfplat, -DSG_MUTANT_x): MFBUF_FP16_NO_ROUND (truncates), MFBUF_WRAP_OFFSET
# (the wrapper ignores its offset), MFBUF_WRAP_NO_BOUNDS, MFBUF_COPY2D_NOOP, MFBUF_BMP_SIZE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mfplatbuf.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/mfplat-buffers-probe.exe" "$HERE/mfplat-buffers-probe.c" -lmfplat -lmfuuid -lole32 -luuid -lstrmiids -lm \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/mfplat-buffers-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" mfplat-buffers-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
