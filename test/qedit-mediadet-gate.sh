#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# qedit's IMediaDet bitmap grabbing (patches/sg/2991): test/qedit-mediadet-probe.c
# writes a small uncompressed AVI, then grabs frames from it with GetBitmapBits at
# several times and sizes (native, doubled, odd), checks the DIB header, pixel
# orientation and scaling, the size query, WriteBitmapBits' file, the sample
# grabber access and leaving grab mode on a stream switch. The run's log is also
# checked: none of the formerly stubbed methods may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/qedit-mediadet-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (qedit, -DSG_MUTANT_x): MD_SEEK (no seek before the grab), MD_STRIDE (rows
# not 4-byte aligned), MD_FLIP (the orientation of the grabbed frame is read backwards), MD_EXIT (a
# stream switch keeps grab mode), MD_FILE (wrong pixel offset in the .bmp).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-qeditmd.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+quartz WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/qedit-mediadet-probe.exe" "$HERE/qedit-mediadet-probe.c" -lole32 -loleaut32 -luuid -lstrmiids \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/qedit-mediadet-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" qedit-mediadet-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:quartz:MediaDet_(GetBitmapBits|WriteBitmapBits|GetSampleGrabber|EnterBitmapGrabMode)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed method logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed methods logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
