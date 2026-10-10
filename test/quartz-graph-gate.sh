#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# quartz's filter graph stubs (patches/sg/2940), on Xvfb: test/quartz-graph-probe.c
# drives a graph with hand written filters and checks IMediaSeeking rate / preroll /
# available, IMediaPosition preroll and CanSeek*, IMediaFilter::GetClassID, the
# IGraphConfig methods (filter cache, filter flags, start time, Reconnect, PushThroughData,
# RemoveFilterEx), IVideoFrameStep forwarding and the IMediaControl collections. The
# run's log is also checked: none of those may log a FIXME any more (the collections
# keep theirs on purpose).
#
#   WINE=/opt/wine-sg/bin/wine test/quartz-graph-gate.sh
# Mutants (quartz, -DSG_MUTANT_x): SEEK_RATE, SEEK_PREROLL, SEEK_AVAIL, CANSEEK,
# CLASSID, CACHE, FLAGS, RECONNECT, REMOVEEX, STEP.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-quartzgraph.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+quartz WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -w -O1 -o "$T/quartz-graph-probe.exe" "$HERE/quartz-graph-probe.c" -lole32 -loleaut32 -luuid -lstrmiids -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/quartz-graph-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" quartz-graph-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:quartz:(MediaSeeking_|MediaPosition_|MediaFilter_GetClassID|GraphConfig_|VideoFrameStep_)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
