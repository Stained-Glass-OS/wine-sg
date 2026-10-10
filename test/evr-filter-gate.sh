#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# evr's DirectShow filter (patches/sg/2892), on Xvfb: test/evr-filter-probe.c uses
# IEVRFilterConfig (number of streams: default, limits, handed to the mixer) and the
# filter's IMediaEventSink (bookkeeping events stay inside, others reach the graph).
#
#   WINE=/opt/wine-sg/bin/wine test/evr-filter-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (evr, -DSG_MUTANT_x): STREAMS_NOLIMIT, STREAMS_NOMIXER, NOTIFY_FORWARDALL, NOTIFY_DROPALL.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-evrfilter.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/evr-filter-probe.exe" "$HERE/evr-filter-probe.c" -I"$HERE" -lmfplat -lmfuuid -lole32 -loleaut32 -luuid -lstrmiids -ldxguid -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/evr-filter-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" evr-filter-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:evr:filter_' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed filter function logged a FIXME"
    exit 1
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
