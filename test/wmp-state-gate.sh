#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wmp's control state (patches/sg/2851): playState/openState/Status through the events, enabled/fullScreen/uiMode/..., the error and closed caption objects, settings (mute, playCount, rate, balance, modes, strings), network (statistics, bufferingTime, per-protocol proxies, sourceProtocol) and controls (isAvailable, pause, fast forward/reverse, position strings) are checked, with a short wave file played through quartz for the playing/paused/stopped states (SKIP lines when no playback is possible).
# Mutants (wmp, -DSG_MUTANT_x): WMP_STATE_NOTRACK, WMP_UIMODE_ANY, WMP_PLAYCOUNT_NOCHECK, WMP_RATE_NOCHECK, WMP_BALANCE_NOCHECK, WMP_MODE_SHARED, WMP_PROXY_SHARED, WMP_BUFTIME_NOCHECK, WMP_AVAIL_ALWAYS, WMP_TIMESTRING.
#
#   WINE=/opt/wine-sg/bin/wine test/wmp-state-gate.sh
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
T=$(mktemp -d /var/tmp/sg-wmp-state.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/wmp-state-probe.exe" "$HERE/wmp-state-probe.c" -lole32 -loleaut32 -luuid -lwininet \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wmp-state-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOR
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" wmp-state-probe.exe 2>/dev/null </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOR
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
