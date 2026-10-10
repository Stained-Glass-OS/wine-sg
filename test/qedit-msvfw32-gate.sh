#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msvfw32's DrawDib / MCIWnd / file-dialog stubs (patches/sg/2994, 2995):
# test/qedit-msvfw32-probe.c checks DrawDibStart/Stop, DrawDibChangePalette,
# DrawDibGetBuffer, DrawDibTime, DrawDibProfileDisplay and the MCIWnd repeat,
# volume, speed, realize and save messages. The run's log is also checked: none
# of those may log a FIXME any more.
#
#   WINE=/opt/wine-sg/bin/wine test/qedit-msvfw32-gate.sh
# Mutants (msvfw32, -DSG_MUTANT_x): VFW_START (DrawDibStart accepts any handle),
# VFW_BUFFER (DrawDibGetBuffer returns NULL), VFW_TIME (DrawDibTime reports nothing),
# VFW_PALETTE (DrawDibChangePalette always succeeds), VFW_REPEAT (MCIWNDM_GETREPEAT
# reports FALSE), VFW_SAVE (MCI_SAVE is swallowed).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-qeditvfw.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+msvideo,fixme+mci WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/qedit-msvfw32-probe.exe" "$HERE/qedit-msvfw32-probe.c" -lmsvfw32 -lwinmm -lgdi32 -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/qedit-msvfw32-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" qedit-msvfw32-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
# none of the formerly stubbed functions may log a FIXME
if grep -E 'fixme:(msvideo|mci):(DrawDib(Start|Stop|ChangePalette|GetBuffer|Realize|Time|ProfileDisplay)|MCIWndProc)|support for MCI' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
