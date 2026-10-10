#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msado15 stream batch (patches/sg/2058): test/adostream.vbs drives ADODB.Stream
# from cscript: text in its character sets, lines, binary data, files and CopyTo.
#
#   WINE=/opt/wine-sg/bin/wine test/adostream-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/msado15/stream.c): SG_MUTANT_STREAM_NOBOM (no byte order mark),
# STREAM_NO_OVERWRITE (SaveToFile always overwrites), STREAM_LINE (a line is
# read to its end whatever the separator), STREAM_NO_WRITEBACK (a file stream
# is never written back).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-adostream.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$HERE/adostream.vbs" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" cscript.exe //nologo adostream.vbs 2>&1 </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo 'FAIL  the script did not finish (an error stops cscript)'
exit 1
