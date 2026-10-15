#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# newdev: DiInstallDriverA/W (driver store + install on present devices, FORCE_INF, older/newer INF; 32-bit gets ERROR_IN_WOW64) (mutants SG_MUTANT_NDV_*)
#
#   WINE=/opt/wine-sg/bin/wine test/fastcloudnewdev-gate.sh
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
T=$(mktemp -d /var/tmp/sg-fastcloudnewdev.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/fastcloudnewdev-probe.exe" "$HERE/fastcloudnewdev-probe.c" -lsetupapi \
    || { echo "FAIL  probe did not build"; exit 1; }
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
command -v "$MINGW32" >/dev/null || { echo "SKIP: $MINGW32 not installed"; exit 77; }
TMPDIR=/var/tmp "$MINGW32" -O1 -o "$T/fastcloudnewdev-probe32.exe" "$HERE/fastcloudnewdev-probe.c" -lsetupapi \
    || { echo "FAIL  32-bit probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/fastcloudnewdev-probe.exe" "$T/fastcloudnewdev-probe32.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" fastcloudnewdev-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
timeout -s KILL 180 "$WINE" fastcloudnewdev-probe32.exe wow64 2>/dev/null </dev/null | tr -d '\r' > "$T/probe32.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out" "$T/probe32.out"
grep -q '^RESULT: PASS' "$T/probe.out" && grep -q '^RESULT: PASS' "$T/probe32.out" && exit 0
exit 1
