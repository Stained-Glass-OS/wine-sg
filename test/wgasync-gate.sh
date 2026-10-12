#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winegstreamer sync reader CLEANPOINT_ONLY + wmvcore async reader Pause/Resume, rate, StartAtPosition, accelerator, type negotiation, protect samples (patches/sg/2955-2956) on test/wgst-wmreader-test.wmv. Fails on any FIXME logged by these paths.
# Mutants: WG_CLEANPOINT (winegstreamer), WMV_NOPAUSE, WMV_RESUMECLOCK, WMV_RATE (wmvcore).
#
#   WINE=/opt/wine-sg/bin/wine test/wmv-reader-gate.sh
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
T=$(mktemp -d /var/tmp/sg-wgasync.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,fixme+wmvcore WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/wgasync-probe.exe" "$HERE/wgasync-probe.c" -lole32 -loleaut32 -luuid -luuid -lstrmiids \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wgasync-probe.exe" "$WINEPREFIX/drive_c/"
[ -z "wgst-wmreader-test.wmv" ] || cp "$HERE/wmv-sample.wmv" "$WINEPREFIX/drive_c/sample.wmv"
cat > "$T/run.sh" <<EOR
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" wgasync-probe.exe C:/sample.wmv 2>"$T/probe.err" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOR
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
# none of the formerly stubbed paths may log a FIXME any more
if grep -q 'fixme:' "$T/probe.err"; then echo "FAIL  a FIXME was logged:"; grep 'fixme:' "$T/probe.err" | sort | uniq -c | head; exit 1; fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
