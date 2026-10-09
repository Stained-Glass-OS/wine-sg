#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# comctl32 odds and ends (patches/sg/1672), on Xvfb: test/ccmisc-probe.c
# uses the date and time picker's app-defined 'X' fields (DTN_FORMATQUERY,
# DTN_FORMAT, DTN_WMKEYDOWN), MirrorIcon, DrawShadowText's soft offset
# shadow, DelMRUString, PSM_RECALCPAGESIZES, a tooltip's WM_NOTIFYFORMAT,
# tab TCIF_RTLREADING and TCS_EX_REGISTERDROP (TCN_GETOBJECT on a real
# drag). These were FIXMEs or stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/ccmisc-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_FORMATQUERY (comctl32/datetime.c),
# SG_MUTANT_NO_MIRROR and SG_MUTANT_FLAT_SHADOW (comctl32/commctrl.c),
# SG_MUTANT_NO_RECALC (comctl32/propsheet.c), SG_MUTANT_NO_TAB_DROP
# (comctl32/tab.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ccmisc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/ccmisc-probe.exe" "$HERE/ccmisc-probe.c" -lcomctl32 -lole32 -luuid -lgdi32 -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/ccmisc-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" ccmisc-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
