#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CDM_SETDEFEXT in the common file dialog (patch 3602); needs the dialog to run on a display.
#   WINE=/path/to/build/wine test/fastb-cdm-gate.sh      SG_XVFB_N=261 to name the display
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-cdm.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -Wno-format -o "$T/probe64.exe" "$HERE/fastb-cdm-probe.c" -lcomdlg32 || { echo "FAIL  probe did not build"; exit 1; }
if command -v i686-w64-mingw32-gcc >/dev/null; then
  TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -Wno-format -o "$T/probe32.exe" "$HERE/fastb-cdm-probe.c" -lcomdlg32 || rm -f "$T/probe32.exe"
fi
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T"/probe*.exe "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
for p in probe64 probe32; do
  [ -f \$p.exe ] || continue
  echo "== \$p"
  timeout -s KILL 240 "$WINE" \$p.exe 2>/dev/null </dev/null | tr -d '\r'
done > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
XN=${SG_XVFB_N:+-n $SG_XVFB_N}
[ -n "$XN" ] || XN=-a
timeout -s KILL 700 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q FAIL "$T/probe.out" && exit 1
[ "$(grep -c '^RESULT: PASS' "$T/probe.out")" -ge 1 ] || exit 1
exit 0
