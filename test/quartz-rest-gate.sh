#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# quartz's remaining stubs (patches 2941-2949), on Xvfb: test/quartz-rest-probe.c
# checks the filter mapper enumerators' Skip/Clone, the video renderer's
# IOverlay, IVideoWindow / IBasicVideo helpers, IAMDirectSound / IQualityControl
# of the DirectSound renderer and IClassFactory::LockServer. No fixme:quartz may
# be logged.
#
#   WINE=/var/tmp/drafter-5/build/obj/wine test/quartz-rest-gate.sh
# Mutants (quartz, -DSG_MUTANT_x): see the patch note.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-quartzrest.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+quartz WINEDLLOVERRIDES="mscoree,mshtml,winedbg.exe=d;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/quartz-rest-probe.exe" "$HERE/quartz-rest-probe.c" -lstrmiids -luuid -lole32 -luser32 -loleaut32 -lgdi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/quartz-rest-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" quartz-rest-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:quartz' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  no fixme:quartz logged"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
