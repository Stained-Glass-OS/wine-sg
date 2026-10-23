#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msado15: a Recordset's changes reach the data source (6037), as ADO's client cursor writes them:
#   - Update (and moving off an edited row) is an UPDATE of the changed columns, the row found by
#     its primary key (the provider's PRIMARY_KEYS schema); AddNew + Update an INSERT; Delete a DELETE
#   - in batch mode (adLockBatchOptimistic) the changes wait for UpdateBatch; CancelBatch takes them back
#   - a row changed by someone else since it was read is a conflict (0x80040E38), not overwritten
#   - a read-only recordset writes nothing
# Before, Update/UpdateBatch returned E_NOTIMPL/E_FAIL and nothing was written.
# The driver is test/adomock-driver.c: one table in a file, the statements logged.
#   WINE=/opt/wine-sg/bin/wine test/adoupdate-gate.sh
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
T=$(mktemp -d /var/tmp/sg-adoupdate.XXXXXX)
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
cp "$HERE/adoupdate.vbs" "$C/"
timeout -s KILL 60 "$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\SG ADO Mock' /v Driver /d 'C:\adomock.dll' /f >/dev/null 2>&1
timeout -s KILL 60 "$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\ODBC Drivers' /v 'SG ADO Mock' /d Installed /f >/dev/null 2>&1
printf '1\tAnn\t30\n2\tBob\t40\n3\tCy\t50\n' > "$C/people.tsv"
export SG_ADOMOCK_DATA='C:\people.tsv' SG_ADOMOCK_LOG='C:\adomock.log'

out=$(cd "$C" && timeout -s KILL 120 "$WINE" cscript //nologo 'C:\adoupdate.vbs' 2>/dev/null </dev/null | tr -d '\r')
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got: $(printf '%s' "$out" | grep "^$1=" || echo nothing)"; fi
}
expect update_err 0 "Update of a changed field succeeds"
expect after_update "1,Anna,30;2,Bob,40;3,Cy,50" "and the table has the new value"
expect move_err 0 "moving off an edited row stores it"
expect after_move "1,Anna,30;2,Bob,41;3,Cy,50" "the table has that row's change"
expect addnew_err 0 "AddNew + Update succeeds"
expect after_addnew "1,Anna,30;2,Bob,41;3,Cy,50;4,Dee,20" "the new row is inserted"
expect delete_err 0 "Delete succeeds"
expect after_delete "2,Bob,41;3,Cy,50;4,Dee,20" "the row is deleted from the table"
expect batch_pending "2,Bob,41;3,Cy,50;4,Dee,20" "in batch mode the changes wait"
expect batch_err 0 "UpdateBatch succeeds"
expect batch_done "2,Bobby,41;4,Dee,20;5,Eve,25" "and writes the change, the deletion and the new row"
expect cancel "0,Bobby" "CancelBatch gives the field its value back"
expect after_cancel "2,Bobby,41;4,Dee,20;5,Eve,25" "and nothing of the cancelled change is written"
expect conflict 80040E38 "a row someone else changed since it was read is a conflict"
expect after_conflict "2,Bobby,77;4,Dee,20;5,Eve,25" "their change is not overwritten"
expect after_readonly "2,Bobby,77;4,Dee,20;5,Eve,25" "a read-only recordset writes nothing"
log=$(tr -d '\r' < "$C/adomock.log" 2>/dev/null)
printf '%s\n' "$log" | grep -q "PRIMARYKEYS" && pass "the table's primary key is asked for" || fail "no PRIMARYKEYS in the log"
printf '%s\n' "$log" | grep -qF "SQL UPDATE People SET Name = ? WHERE Id = ? AND Name = ? | 1:'Anna' | 2:1 | 3:'Ann'" &&
    pass "the UPDATE has the changed column and finds the row by its key and the changed column's old value" ||
    fail "the UPDATE: $(printf '%s\n' "$log" | grep 'UPDATE' | head -2 | tr '\n' '|')"
printf '%s\n' "$log" | grep -qF "SQL INSERT INTO People (Id, Name, Age) VALUES (?, ?, ?) | 1:4 | 2:'Dee' | 3:20" &&
    pass "the INSERT has the new row's values" ||
    fail "the INSERT: $(printf '%s\n' "$log" | grep 'INSERT' | head -1)"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
