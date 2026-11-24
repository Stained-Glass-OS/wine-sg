#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msado15: a saved recordset is opened again (wine-sg 9454). Recordset.Save wrote ADO's XML format, but
# Recordset.Open could not read it back: a file name without a connection was "Unsupported Active
# connection type", Provider=MSPersist was an unknown provider and a Stream was an invalid argument. As
# ADO does, Open of a file (adCmdFile, Provider=MSPersist, or a file name without a connection) or of a
# Stream reads the schema's fields and the rows:
#   - the field types come back (currency, dates, Booleans, ANSI and Unicode text), Null values are Null
#   - a disconnected, static, client-cursor recordset with batch updates; a change can be saved again
#   - a file as Windows writes it: a field with rs:name, a timestamp, the changes pending in a batch
#     (rs:update with rs:original, rs:insert, rs:delete) come back pending
#   - a missing file and a file that is not XML are errors
#   WINE=/opt/wine-sg/bin/wine test/adopersist-gate.sh
# Mutant: SG_MUTANT_ADO_NO_PERSIST (recordset.c: Open does not read persisted recordsets).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-adopersist.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$HERE/adopersist.vbs" "$C/"

out=$(cd "$C" && timeout -s KILL 120 "$WINE" cscript //nologo 'C:\adopersist.vbs' 2>/dev/null </dev/null | tr -d '\r')
[ -n "${VERBOSE:-}" ] && printf '%s\n' "$out"
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got: $(printf '%s' "$out" | grep "^$1=" || echo nothing)"; fi
}
expect save 0 "a recordset is saved as XML"
expect open "0,1,3" "Open of the file without a connection: open, its rows"
expect fields "6,Id,Name,Code" "its fields"
expect types "3,202,6,7,11,200" "their types: integer, Unicode text, currency, date, Boolean, ANSI text"
expect row1 "1,Ann,12.5,1990-5-17 8:30,True,a1" "the values"
expect row2 "2,True,False,Null" "a Null currency is Null, False is False"
expect row3 "Cy & <x> 'q',0.25,True" "text with XML's special characters, a Null text"
expect cursor "3,3,4" "a client cursor, static, batch optimistic"
expect edit 0 "a value can be changed"
expect mspersist "0,3,Anne" "Provider=MSPersist with adCmdFile opens the file saved after the change"
expect save_stream 0 "saved to a Stream"
expect stream "0,3,Ann" "and opened from the Stream"
expect win_open "0,3" "a file as Windows writes it: its rows (the deleted one pending, not shown)"
expect win_fields "Full Name,200,50,6,135" "rs:name, ANSI text and its size, currency, timestamp"
expect win_row1 "Ann,12.5,2024-2-29 13:45:10" "its values"
expect win_updated "2,Bob,3,Bo" "a pending change: the new value, the others kept, the original value"
expect win_inserted "3,Cy" "a pending new row"
expect win_pending 2 "the changed and the new row are pending"
expect missing "True,0" "a missing file is an error"
expect garbage "True,0" "a file that is not XML is an error"
expect done 1 "the script ran to the end"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
