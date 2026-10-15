#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wbemdisp: the scripting object model (properties, methods, qualifiers, paths, privileges, name/value sets)
#
#   WINE=/opt/wine-sg/bin/wine test/fastwbemdisp-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastwbemdisp.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$HERE/fastwbemdisp.vbs" "$WINEPREFIX/drive_c/t.vbs"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" cscript //nologo 'C:\\t.vbs' >"$T/probe.out" 2>/dev/null </dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
tr -d '\r' < "$T/probe.out" | grep -v '^PASS' 
EXPECT=$(grep -c '^ *Check "' "$HERE/fastwbemdisp.vbs")
GOT=$(grep -c '^PASS' "$T/probe.out")
[ "$EXPECT" = "$GOT" ] || echo "FAIL  $GOT of $EXPECT checks ran"
[ "$EXPECT" = "$GOT" ] && grep -q '^RESULT: PASS' "$T/probe.out" && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"
exit 1
