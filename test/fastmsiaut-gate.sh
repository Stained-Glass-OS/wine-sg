#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msi automation members (patch 3218): test/fastmsiaut.vbs drives WindowsInstaller.Installer through cscript, 64-bit and 32-bit:
# Database, View, Record, Session, UIPreview, FeatureInfo, SummaryInfo, RecordList, Product, Patch and the Installer members that were missing from the type library.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmsiaut-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastmsiaut.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$HERE/fastmsiaut.vbs" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 600 "$WINE" cscript.exe //nologo fastmsiaut.vbs 2>&1 </dev/null | tr -d '\\r' > "$T/probe64.out"
timeout -s KILL 600 "$WINE" 'C:\\windows\\syswow64\\cscript.exe' //nologo fastmsiaut.vbs 2>&1 </dev/null | tr -d '\\r' > "$T/probe32.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 1500 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
rc=0
for a in 64 32; do
    echo "---- $a-bit"
    grep -v '^PASS' "$T/probe$a.out" | grep -v 'fixme:' | head -40
    echo "$(grep -c '^PASS' "$T/probe$a.out") checks passed"
    grep -q '^RESULT: PASS' "$T/probe$a.out" || { grep -q '^RESULT:' "$T/probe$a.out" || echo "FAIL  the $a-bit script did not finish (an error stops cscript)"; rc=1; }
done
[ $rc = 0 ] && echo "RESULT: PASS"
exit $rc
