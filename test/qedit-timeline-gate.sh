#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# qedit's timeline object model (patches/sg/2990): test/qedit-timeline-probe.c
# builds a timeline, groups and plain nodes and checks defaults, ranges,
# ordering, dirty tracking, ids, names, user data, sub-objects, media types and
# the E_POINTER / E_INVALIDARG / E_NOINTERFACE answers. The run's log is also
# checked: none of the formerly stubbed timeline methods may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/qedit-timeline-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (qedit, -DSG_MUTANT_x): TL_ORDER (groups go to the front of the list),
# TL_DIRTY (SetStartStop does not dirty), TL_RANGE (no E_INVALIDARG on bad times),
# TL_FIX (FixTimes does not snap), TL_GUID (default GUIDs are not stored),
# TL_REFS (a group list entry holds no reference), TL_USERDATA (user data is not copied).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-qedittl.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+quartz WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/qedit-timeline-probe.exe" "$HERE/qedit-timeline-probe.c" -lole32 -loleaut32 -luuid -lstrmiids \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/qedit-timeline-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" qedit-timeline-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:quartz:(Timeline|timelinegrp)' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed timeline method logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed timeline methods logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
