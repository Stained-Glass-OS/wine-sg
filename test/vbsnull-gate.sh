#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# vbscript IsNull/IsEmpty/IsDate/VarType of an object with a default property
# (patches/sg/9610): test/vbsnull.vbs opens a real ADODB.Recordset with NULL
# columns and tests IsNull(rs("name")) the way ASP pages and admin scripts do;
# Windows tests the Field's default (Value), Wine tested the object.
# cscript needs no display: the gate runs headless.
#
#   WINE=/opt/wine-sg/bin/wine test/vbsnull-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant (dlls/vbscript/global.c): SG_MUTANT_VBS_NO_DEFAULT_VALUE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-vbsnull.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$HERE/vbsnull.vbs" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" cscript.exe //nologo vbsnull.vbs 2>&1 </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 env DISPLAY= "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo 'FAIL  the script did not finish (an error stops cscript)'
exit 1
