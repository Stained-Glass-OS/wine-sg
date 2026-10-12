#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mfplat media type conversions (patches/sg/2965): MFInitMediaTypeFromMFVideoFormat (overflow, chroma-block
# strides), MFInitMediaTypeFromAMMediaType (empty type, MFVIDEOFORMAT block, audio subtype), via
# test/mfplat-mtconv-probe.c.
#
#   WINE=/opt/wine-sg/bin/wine test/mfplat-mtconv-gate.sh
# Mutants (mfplat, -DSG_MUTANT_x): MFVF_NO_OVERFLOW, MFVF_STRIDE_RAW, AMMT_NO_MFVF, AMMT_NO_NULL, AMMT_TAG_SUBTYPE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mfmtconv.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/mfplat-mtconv-probe.exe" "$HERE/mfplat-mtconv-probe.c" -lmfplat -lmfuuid -lole32 -luuid -lm \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/mfplat-mtconv-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" mfplat-mtconv-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
DISPLAY= timeout -s KILL 300 "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:mfplat:' "$T/stderr.log"; then
    echo "FAIL  mfplat logged a FIXME"
    exit 1
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT' "$T/probe.out" || echo "FAIL  the probe produced no result (crashed?)"
exit 1
