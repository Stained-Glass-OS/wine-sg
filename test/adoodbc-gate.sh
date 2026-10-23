#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msado15: ADO over ODBC (MSDASQL) as scripts use it (6032):
#   - an open connection keeps its data source initialized: every command after Open failed
#     ("not connected") because ADO uninitialized the data source at once
#   - a SELECT's rows are read even when the driver's row count is -1 (most drivers'): the recordset
#     was empty
#   - rs("Name") and rs("Name") = value are the field's value (Collect)
#   - Recordset.Open takes a connection passed by reference (VT_BYREF | VT_VARIANT, as VBScript passes it)
# The driver is test/adomock-driver.c: one table in a file, the statements logged.
#   WINE=/opt/wine-sg/bin/wine test/adoodbc-gate.sh
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
T=$(mktemp -d /var/tmp/sg-adoodbc.XXXXXX)
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
cp "$HERE/adoodbc.vbs" "$C/"
timeout -s KILL 60 "$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\SG ADO Mock' /v Driver /d 'C:\adomock.dll' /f >/dev/null 2>&1
timeout -s KILL 60 "$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\ODBC Drivers' /v 'SG ADO Mock' /d Installed /f >/dev/null 2>&1
printf '1\tAnn\t30\n2\tBob\t40\n' > "$C/people.tsv"
export SG_ADOMOCK_DATA='C:\people.tsv' SG_ADOMOCK_LOG='C:\adomock.log'

out=$(cd "$C" && timeout -s KILL 120 "$WINE" cscript //nologo 'C:\adoodbc.vbs' 2>/dev/null </dev/null | tr -d '\r')
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got: $(printf '%s' "$out" | tr '\n' ' ')"; fi
}
expect open 0 "Connection.Open of an ODBC driver"
expect execute 0 "Connection.Execute of a SELECT works after Open"
expect row "1,Ann,30" "the recordset has the first row"
expect row "2,Bob,40" "and the second"
expect rows 2 "two rows (the driver's row count is -1)"
expect open_byref 0 "Recordset.Open with the connection passed by reference"
expect collect Bob "rs(\"Name\") is the field's value"
expect collect_err 0 "without an error"
expect collect_put_err 0 "rs(\"Age\") = 5 sets the field's value"
expect collect_put 5 "which reads back"
expect second_execute 0 "a later command still runs on the connection"
expect close 0 "Close"
log=$(tr -d '\r' < "$C/adomock.log" 2>/dev/null)
n_before=$(printf '%s\n' "$log" | sed -n '/DELETE FROM People/q;p' | grep -c DISCONNECT)
[ "$n_before" = 0 ] && pass "the driver is not disconnected while the connection is open" ||
    fail "the driver was disconnected before the last command: $(printf '%s' "$log" | tr '\n' '|')"
printf '%s\n' "$log" | tail -1 | grep -qx DISCONNECT && pass "Close disconnects it" ||
    fail "the log does not end with DISCONNECT: $(printf '%s' "$log" | tr '\n' '|')"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
