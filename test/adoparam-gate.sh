#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msado15, msdasql: ADO commands with parameters (6034). Command.Execute refused its Parameters and
# the values given to Execute (adErrFeatureNotAvailable), and MSDASQL ignored a command's DBPARAMS;
# now each parameter is given to the ODBC driver with SQLBindParameter, output and return values come
# back, and a stored procedure is an ODBC call. The driver is test/adomock-driver.c.
#   WINE=/opt/wine-sg/bin/wine test/adoparam-gate.sh
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
T=$(mktemp -d /var/tmp/sg-adoparam.XXXXXX)
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
cp "$HERE/adoparam.vbs" "$C/"
timeout -s KILL 60 "$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\SG ADO Mock' /v Driver /d 'C:\adomock.dll' /f >/dev/null 2>&1
timeout -s KILL 60 "$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\ODBC Drivers' /v 'SG ADO Mock' /d Installed /f >/dev/null 2>&1
printf '1\tAnn\t30\n2\tBob\t40\n' > "$C/people.tsv"
export SG_ADOMOCK_DATA='C:\people.tsv' SG_ADOMOCK_LOG='C:\adomock.log'

out=$(cd "$C" && timeout -s KILL 120 "$WINE" cscript //nologo 'C:\adoparam.vbs' 2>/dev/null </dev/null | tr -d '\r')
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got: $(printf '%s' "$out" | tr '\n' ' ')"; fi
}
expect select_err 0 "a SELECT with an integer parameter runs"
expect select "Bob|1" "and gives the row it selects"
expect reexecute 2 "the command runs again with the parameter's new value"
expect insert_err "0 affected=1" "an INSERT with the values given to Execute (a quote and a NULL among them)"
expect update_err "0 affected=1" "an UPDATE with a string parameter finds the row"
expect proc_err 0 "a stored procedure with a return value and an in/out parameter"
expect proc_return 3 "its return value comes back (the three people)"
expect proc_inout 105 "and its in/out parameter's new value"
expect proc_rows 3 "a stored procedure that returns rows gives a recordset"
expect row3 "3,O'Hara,77" "the inserted and updated row is in the table"
log=$(tr -d '\r' < "$C/adomock.log" 2>/dev/null)
printf '%s\n' "$log" | grep -qF "SQL SELECT Name FROM People WHERE Age > ? | 1:35" && pass "the driver got the statement with the parameter bound" ||
    fail "the log: $(printf '%s' "$log" | tr '\n' '|')"
printf '%s\n' "$log" | grep -qF "| 1:3 | 2:'O'Hara' | 3:NULL" && pass "the values' types (integer, text, NULL)" ||
    fail "the INSERT's parameters: $(printf '%s' "$log" | grep INSERT)"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
