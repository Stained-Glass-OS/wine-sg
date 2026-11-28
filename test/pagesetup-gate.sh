#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Page Setup dialog in Windows' words (patches/sg/9820): "Page Setup",
# not "Setup Page"; "Margins (inches)" / "Margins (millimeters)", not
# "Margins [inches]" / "Margins [mm]". test/pagesetup-probe.c under Xvfb.
#
#   WINE=/opt/wine-sg/bin/wine test/pagesetup-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-pagesetup.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/pagesetup-probe.c" -lcomdlg32 || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 60 "$WINE" 'C:\\probe.exe' 2>/dev/null </dev/null | tr -d '\r' > "$T/mm.out"
timeout -s KILL 60 "$WINE" 'C:\\probe.exe' inches 2>/dev/null </dev/null | tr -d '\r' > "$T/in.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 200 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
sed 's/^/      /' "$T/mm.out" "$T/in.out"
if grep -q '^RESULT: PASS' "$T/mm.out" && grep -q '^RESULT: PASS' "$T/in.out"; then
    echo "PASS  Page Setup speaks Windows' words"; echo "RESULT: PASS"; exit 0
fi
echo "FAIL  Page Setup's words"; echo "RESULT: FAIL"; exit 1
