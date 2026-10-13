#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# twinapi.appcore + windows.applicationmodel: IInspectable answers, PropertySet events, Package identity
#
#   WINE=/opt/wine-sg/bin/wine test/fastappmodel-gate.sh
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
T=$(mktemp -d /var/tmp/sg-fastappmodel.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/fastappmodel-probe.exe" "$HERE/fastappmodel-probe.c" -lole32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$WINEPREFIX/drive_c/pkg"
cp "$T/fastappmodel-probe.exe" "$WINEPREFIX/drive_c/"
cp "$T/fastappmodel-probe.exe" "$WINEPREFIX/drive_c/pkg/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" fastappmodel-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
# the same program, now inside an installed package (its image is in the package's location)
timeout -s KILL 60 "$WINE" reg add 'HKCU\\Software\\Wine\\AppModel\\Packages\\Fabrikam.Test_1.2.3.4_x64__8wekyb3d8bbwe' /v InstallLocation /t REG_SZ /d 'C:\\pkg' /f >/dev/null 2>&1
timeout -s KILL 180 "$WINE" pkg/fastappmodel-probe.exe packaged 2>/dev/null </dev/null | tr -d '\r' > "$T/probe2.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out" "$T/probe2.out"
grep -q '^RESULT: PASS' "$T/probe.out" && grep -q '^RESULT: PASS' "$T/probe2.out" && exit 0
exit 1
