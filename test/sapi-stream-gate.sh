#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SAPI streams, audio formats and the resource manager (patches/sg/2840), on
# Xvfb: test/sapi-stream-probe.c drives ISpStream over memory and wave/raw
# files (SetBaseStream, BindToFile, Read/Write/Seek/SetSize/CopyTo/Clone/Stat,
# the RIFF header on disk), the per-process ISpResourceManager (SetObject,
# GetObject, QueryService), and ISpeechFileStream with the ISpeechAudioFormat /
# ISpeechWaveFormatEx objects late-bound through IDispatch. Before the patch all
# of these were FIXME stubs returning E_NOTIMPL.
#
#   WINE=/opt/wine-sg/bin/wine test/sapi-stream-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (sapi, -DSG_MUTANT_x): SAPISTR_CLOSE_KEEPS (Close keeps the base
# stream), SAPISTR_NO_HEADER_FIX (RIFF sizes never rewritten), SAPISTR_RAW_OFFSET
# (reads include the header), SAPISTR_REINIT (second SetBaseStream accepted),
# SAPIRES_NO_SINGLETON, SAPIRES_SETNULL_KEEPS, SAPIAUT_TYPE_STEREO (Type maps
# stereo as mono), SAPIAUT_SEEK_ORIGIN (origin ignored), SAPIAUT_DEFAULT_RATE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sapistream.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/sapi-stream-probe.exe" "$HERE/sapi-stream-probe.c" -lsapi -luuid -lole32 -loleaut32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/sapi-stream-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" sapi-stream-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
