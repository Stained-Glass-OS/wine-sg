#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# scrobj: a script component (variable property, internalName, resource, object, createComponent) and the
# Scriptlet.TypeLib generator
#
#   WINE=/opt/wine-sg/bin/wine test/fastscrobj-gate.sh
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
T=$(mktemp -d /var/tmp/sg-fastscrobj.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastscrobj-probe.c" -lole32 -loleaut32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$HERE/fastscrobj.wsc" "$HERE/fastscrobj.vbs" "$T/probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 60 "$WINE" regsvr32 /s /n /i:'C:\\fastscrobj.wsc' scrobj.dll >/dev/null 2>&1 </dev/null
timeout -s KILL 120 "$WINE" cscript //nologo 'C:\\fastscrobj.vbs' >"$T/vbs.out" 2>/dev/null </dev/null
timeout -s KILL 60 "$WINE" probe.exe >"$T/probe.out" 2>/dev/null </dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
tr -d '\r' < "$T/vbs.out" | grep -v '^PASS'
tr -d '\r' < "$T/probe.out"
EXPECT=$(grep -c '^ *Check "' "$HERE/fastscrobj.vbs")
GOT=$(grep -c '^PASS' "$T/vbs.out")
[ "$EXPECT" = "$GOT" ] || echo "FAIL  $GOT of $EXPECT checks ran"
[ "$EXPECT" = "$GOT" ] && grep -q '^RESULT: PASS' "$T/vbs.out" && grep -q '^RESULT: PASS' "$T/probe.out" && { echo "GATE: PASS"; exit 0; }
echo "GATE: FAIL"
exit 1
