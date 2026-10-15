#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# setupapi gap-A batch (patch 3212): INF UpdateIniFields / Ini2Reg / BitReg / CopyINF, SetupOpenInfFile class,
# original INF name and driver store location, SetupDiGetINFClass search, file queue alternate platform,
# Setup*Error prompts, CM_Create_DevNode, co-installer post-processing.
#
#   WINE=/opt/wine-sg/bin/wine test/fastsetupapi-gate.sh      (MINGW=i686-w64-mingw32-gcc for 32 bit)
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/setupapi): SG_MUTANT_SUAPI_* (UIF OVERWRITE BITREG COPYINF NOSKIP ALTSIZE ORIGNAME CLASS POST DUP INFSEARCH).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastsetupapi.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG="${SG_GATE_DEBUG:--all}" WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/fastsetupapi-probe.exe" "$HERE/fastsetupapi-probe.c" -lsetupapi -ladvapi32 -luser32 -lshlwapi -lole32 \
    || { echo "FAIL  probe did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O1 -shared -Wl,--kill-at -o "$T/sgcoinst.dll" "$HERE/fastsetupapi-coinst.c" -ladvapi32 \
    || { echo "FAIL  co-installer did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/fastsetupapi-probe.exe" "$T/sgcoinst.dll" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" fastsetupapi-probe.exe 'C:\\sgtest' 'C:\\sgcoinst.dll' 2>"$T/err" </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 400 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
[ -n "${SG_GATE_DEBUG:-}" ] && grep -v "^$" "$T/err" | grep -i "copying\|CopyOEM\|inf directory" | head -30
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
