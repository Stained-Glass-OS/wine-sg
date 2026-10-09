#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mfplat's byte stream and property store stubs (patches/sg/2831), on Xvfb:
# test/mfplat-bytestream-probe.c uses a file byte stream's SetLength, Flush,
# Close and IMFGetService, MFCreateMFByteStreamOnStreamEx and the property
# store's Commit, and checks the files and values they leave behind.
#
#   WINE=/opt/wine-sg/bin/wine test/mfplat-bytestream-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (mfplat, -DSG_MUTANT_x): MFBS_SETLENGTH_NOOP, MFBS_CLOSE_KEEPS_HANDLE,
# MFBS_EX_ACCEPTS_ANY, MFBS_SERVICE_OK.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mfplatbs.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/mfplat-bytestream-probe.exe" "$HERE/mfplat-bytestream-probe.c" -lmfplat -lmfuuid -lole32 -luuid -lstrmiids -lm \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/mfplat-bytestream-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" mfplat-bytestream-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
