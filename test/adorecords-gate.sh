#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msado15 recordset batch (patches/sg/2043): test/adorecords.vbs drives an
# in-memory ADODB.Recordset from cscript: AbsolutePosition, Move, Find, Sort,
# GetRows, GetString, Delete, paging, Source, Status and Update with a field.
#
#   WINE=/opt/wine-sg/bin/wine test/adorecords-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/msado15/recordset.c): SG_MUTANT_ADO_FIND (Find does not look),
# ADO_SORT (Sort is ignored), ADO_GETSTRING (the row delimiter is left off the
# last row), ADO_MOVE (Move does not clamp), ADO_BYREF (a value given by a variable is stored as a reference), ADO_RESIZE (a recordset with
# several columns clears the wrong memory when it grows).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-adorecords.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$HERE/adorecords.vbs" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" cscript.exe //nologo adorecords.vbs 2>&1 </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo 'FAIL  the script did not finish (an error stops cscript)'
exit 1
