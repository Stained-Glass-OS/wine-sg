#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml the batch-4 mshtml behaviours (patch 3192): text ranges, status/indeterminate, form+select item+tags, URL file/mime/protocol props, frame+object props, location puts, comments, attribute nodes, current style, table, style element, HTML dialogs.
# Mutants: SG_MUTANT_B4_RANGE, _ITEM, _URL, _STYLE, _TABLE, _ATTR, _LOC, _DLG, _SCRIPTDLG, _HTA
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlb4-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastmshtmlb4.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/fastmshtmlb4-probe.exe" "$HERE/fastmshtmlb4-probe.c" -lole32 -loleaut32 -luuid -lurlmon -lshlwapi \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/fastmshtmlb4-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" fastmshtmlb4-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
