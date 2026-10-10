#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winegstreamer's media_source.c / quartz_parser.c remaining stubs
# (patches/sg/2922), on Xvfb: the probe test/wgrest-source-probe.c checks the
# byte stream handler (bytes needed for resolution, resolution flags) and
# IAMStreamSelect of the MPEG splitter (test/wgrest-test.mpg; Info flags /
# group / name / object, Enable one / none / all).
#
#   WINE=/opt/wine-sg/bin/wine test/wgrest-source-gate.sh
# Mutants (winegstreamer, -DSG_MUTANT_x): SELNOOP (Enable cannot deselect), HANDLERBYTES (0 bytes needed), SELFLAGS (Info
# reports no enabled flag), SELGROUP (every stream in one group).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wgrestsrc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+mfplat,fixme+quartz WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -Wno-format -o "$T/wgrest-source-probe.exe" "$HERE/wgrest-source-probe.c" -lole32 -luuid -luser32 -lmfplat -lmf -lmfuuid -lstrmiids \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wgrest-source-probe.exe" "$WINEPREFIX/drive_c/"
cp "$HERE/wgrest-test.mpg" "$WINEPREFIX/drive_c/test.mpg"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" wgrest-source-probe.exe 'C:\\test.mpg' 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:(mfplat|quartz|wmvcore):(stream_select_|stream_handler_BeginCreateObject|stream_handler_GetMax|mf_media_type_from_wg)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
