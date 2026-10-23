#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msado15/msdasql: long text columns (6038). A column the driver reports as SQL_WLONGVARCHAR (SQLite's
# TEXT, a memo) is bound by ADO as a DBTYPE_IUNKNOWN storage object (ISequentialStream); MSDASQL
# refused that binding, so the whole recordset failed to load and every value read empty.
#   - the value reads, Null reads as Null, through Execute and Recordset.Open
#   - the field is adLongVarWChar (203) with adFldLong
#   - a long column can be changed and written back
# The driver is test/adomock-driver.c with SG_ADOMOCK_LONG=1.
#   WINE=/opt/wine-sg/bin/wine test/adolong-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-adolong.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -shared -o "$T/adomock.dll" "$HERE/adomock-driver.c" || { fail "the driver did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/adomock.dll" "$C/"
cp "$HERE/adolong.vbs" "$C/"
timeout -s KILL 60 "$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\SG ADO Mock' /v Driver /d 'C:\adomock.dll' /f >/dev/null 2>&1
timeout -s KILL 60 "$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\ODBC Drivers' /v 'SG ADO Mock' /d Installed /f >/dev/null 2>&1
printf '1\tAnn\t30\n2\t\\N\t40\n3\tCy\t50\n' > "$C/people.tsv"
export SG_ADOMOCK_DATA='C:\people.tsv' SG_ADOMOCK_LOG='C:\adomock.log' SG_ADOMOCK_LONG=1

out=$(cd "$C" && timeout -s KILL 120 "$WINE" cscript //nologo 'C:\adolong.vbs' 2>/dev/null </dev/null | tr -d '\r')
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got: $(printf '%s' "$out" | grep "^$1=" || echo nothing)"; fi
}
expect exec_err 0 "a SELECT with a long text column runs"
expect value Ann "the long column's value reads"
expect type 203 "the field is adLongVarWChar"
expect long True "with adFldLong"
expect null True "a Null long value is Null"
expect third Cy "the next rows read too"
expect open_err 0 "Recordset.Open of the table"
expect update_err 0 "the long column is changed and written back"
expect updated Anne "the table has the new value"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
