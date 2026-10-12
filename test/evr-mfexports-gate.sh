#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# evr's mfplat-equivalent exports (patches/sg/2945): test/evr-mfexports-probe.c looks each up in
# evr.dll with GetProcAddress and checks values (DXVA colour info, FP16 arrays, media types
# from BITMAPINFOHEADER/VIDEOINFOHEADER2, MFInitVideoFormat(_RGB), MFGetUncompressedVideoFormat).
#
#   WINE=/opt/wine-sg/bin/wine test/evr-mfexports-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: evr.spec entries reverted to "@ stub" (test/evr-mfexports-mutants.sh).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-evrmfexp.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe,winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/evr-mfexports-probe.exe" "$HERE/evr-mfexports-probe.c" -I"$HERE" -lmfplat -lmfuuid -lole32 -loleaut32 -luuid -lstrmiids -ldxguid -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/evr-mfexports-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" evr-mfexports-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:(evr|mfplat):|unimplemented function' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed export logged a FIXME or was an unimplemented stub"
    exit 1
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
