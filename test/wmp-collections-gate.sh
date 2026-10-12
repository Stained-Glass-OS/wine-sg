#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wmp's collection and application objects (patches/sg/2946-2949): mediaCollection, playlistCollection (with the empty string collection and playlist array), cdromCollection, dvd, playerApplication and the network buffering/download progress, run on Xvfb (test/wmp-collections-probe.c). Values, HRESULTs, E_POINTER/E_INVALIDARG cases and IDispatch identity are checked; the probe's stderr must carry no wmp FIXME.
# Mutants (wmp, -DSG_MUTANT_x): WMP_EMPTYLIST_COUNT, WMP_MEDIACOLL_NOALL, WMP_CDROM_ITEM_OK, WMP_DVD_AVAILABLE, WMP_PLAYERAPP_DOCKED, WMP_BUFFER_ZERO, WMP_DOWNLOAD_ZERO.
#
#   WINE=/opt/wine-sg/bin/wine test/wmp-collections-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wmp-collections.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+wmp WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/wmp-collections-probe.exe" "$HERE/wmp-collections-probe.c" -lole32 -loleaut32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wmp-collections-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOR
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" wmp-collections-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOR
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
# nothing in wmp may log a FIXME any more
if grep -E 'fixme:wmp' "$T/stderr.log"; then
    echo "FAIL  wmp logged a FIXME"
    exit 1
fi
echo "PASS  wmp logged no FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
