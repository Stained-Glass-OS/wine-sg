#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wmvcore's reader configuration (patches/sg/2854): IWMReaderNetworkConfig2 settings, string getters, UDP port ranges and logging URLs; the reader-advanced state flags, play mode, statistics and client info; IReferenceClock advises; IWMReaderStreamClock timers on an open reader with the program's clock and the reader's (needs test/wmv-sample.wmv, SKIPs when it cannot be opened).
# Mutants (wmvcore, -DSG_MUTANT_x): WMV_STRLEN, WMV_PROXY_SHARED, WMV_PLAYMODE_NOCHECK, WMV_USERCLOCK_EARLY, WMV_UNADVISE_NOOP, WMV_UDP_NOCHECK, WMV_BOOL_RAW.
#
#   WINE=/opt/wine-sg/bin/wine test/wmv-config-gate.sh
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
T=$(mktemp -d /var/tmp/sg-wmv-config.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/wmv-config-probe.exe" "$HERE/wmv-config-probe.c" -lole32 -loleaut32 -luuid -luuid -lstrmiids \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wmv-config-probe.exe" "$WINEPREFIX/drive_c/"
[ -z "wmv-sample.wmv" ] || cp "$HERE/wmv-sample.wmv" "$WINEPREFIX/drive_c/sample.wmv"
cat > "$T/run.sh" <<EOR
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" wmv-config-probe.exe C:/sample.wmv 2>/dev/null </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOR
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
