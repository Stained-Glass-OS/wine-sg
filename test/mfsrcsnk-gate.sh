#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mfsrcsnk's WAVE media sink members that were stubs (patches/sg/2872), on Xvfb:
# test/mfsrcsnk-probe.c drives a WAVE sink on a temporary file (stream sink
# PlaceMarker / Flush, the media type handler, OnClockSetRate, the class
# factory) before and after Shutdown; the gate fails when a formerly stubbed
# function logs a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/mfsrcsnk-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (mfsrcsnk, -DSG_MUTANT_x): SNK_MARKER_CONTEXT, SNK_MARKER_RANGE, SNK_TYPE_CHECK, SNK_TYPE_APPLY, SNK_TYPE_LOCK, SNK_SHUTDOWN.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mfsrcsnk.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=err+all,fixme+all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/mfsrcsnk-probe.exe" "$HERE/mfsrcsnk-probe.c" -lmfplat -lmfuuid -lole32 -loleaut32 -luuid -lm \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/mfsrcsnk-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" mfsrcsnk-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:mfplat:(wave_stream_sink_(PlaceMarker|Flush)|wave_sink_clock_sink_OnClockSetRate|wave_sink_type_handler_)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
