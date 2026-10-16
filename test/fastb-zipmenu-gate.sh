#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# shell32: GetCommandString of the context menu of an item in a compressed folder (patch 3702)
#
#   WINE=/opt/wine-sg/bin/wine test/fastb-zipmenu-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
#   SG_XVFB_N=260 to name the display
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: needs python3"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-zipmenu.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastb-zipmenu-probe.c" -lole32 -luuid -lshlwapi -lshell32 -loleaut32 -lgdi32 -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
mkdir -p "$WINEPREFIX/drive_c/sgz"
python3 - "$WINEPREFIX/drive_c/sgz/test.zip" <<'EOF'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1], 'w') as z:
    z.writestr('readme.txt', 'hello zip\r\n' * 50, compress_type=zipfile.ZIP_DEFLATED)
    z.writestr('docs/deep.txt', 'deep', compress_type=zipfile.ZIP_DEFLATED)
EOF
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 300 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
