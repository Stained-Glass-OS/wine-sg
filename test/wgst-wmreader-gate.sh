#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winegstreamer's wm_reader.c stubs (patches/sg/2860), on Xvfb: the probe
# test/wgst-wmreader-probe.c opens test/wgst-wmreader-test.wmv in a sync reader
# and checks IWMOutputMediaProps group / connection names, INSSBuffer::GetLength,
# IWMStreamConfig (name, connection, bitrate, buffer window, number, media
# type), IWMProfile (name, description, version, storage format, streams
# add / remove / reconfigure / create, mutual exclusions, bandwidth sharing,
# stream prioritization, expected packet count). The run's log must not hold
# a FIXME from those functions.
#
#   WINE=/opt/wine-sg/bin/wine test/wgst-wmreader-gate.sh
# Mutants (winegstreamer, -DSG_MUTANT_x): STRLEN (string length without the NUL),
# BUFLEN (GetLength returns the capacity), NUMRANGE (stream number 64 accepted),
# NORECONFIG (ReconfigStream stores nothing), REMOVE (RemoveStream is a no-op),
# ADDDUP (AddStream accepts a number twice), LISTDUP (stream lists accept a
# number twice), PRIOSTALE (SetPriorityRecords keeps the old records).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wgstwmr.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+wmvcore WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -Wno-format -o "$T/wgst-wmreader-probe.exe" "$HERE/wgst-wmreader-probe.c" -lole32 -luuid -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wgst-wmreader-probe.exe" "$WINEPREFIX/drive_c/"
cp "$HERE/wgst-wmreader-test.wmv" "$WINEPREFIX/drive_c/test.wmv"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" wgst-wmreader-probe.exe 'C:\\test.wmv' 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:wmvcore:(output_props_Get|buffer_GetLength|stream_config_|stream_props_|profile_)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
