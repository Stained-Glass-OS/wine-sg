#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# quartz patches 2931-2935, on Xvfb: test/quartz-collections-probe.c checks IMediaControl::AddSourceFilter,
# FilterCollection and RegFilterCollection (IAMCollection, IFilterInfo, IRegFilterInfo, IPinInfo,
# IMediaTypeInfo) against the plain IBaseFilter/IPin/IFilterMapper interfaces. No fixme:quartz may be logged.
#
#   WINE=/var/tmp/drafter-5/build/obj/wine test/quartz-collections-gate.sh
# Mutants (quartz, -DSG_MUTANT_x): see the list in the patch note.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-quartzcoll.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+quartz WINEDLLOVERRIDES="mscoree,mshtml,winedbg.exe=d;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/quartz-collections-probe.exe" "$HERE/quartz-collections-probe.c" -lstrmiids -luuid -lole32 -luser32 -loleaut32 -lgdi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/quartz-collections-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" quartz-collections-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
# mpeg_splitter_sink_query_accept is logged by the unrelated MPEG splitter while IPinInfo::Connect
# runs the graph's intelligent connection; it is not one of the formerly stubbed functions.
if grep -E 'fixme:quartz' "$T/stderr.log" | grep -v mpeg_splitter_sink_query_accept; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  no fixme:quartz logged"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
