#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msado15: Recordset.Filter (wine-sg 9450). A criteria string hid every row ("No filter performed.
# Reporting no records found."), so a script or program that filters a recordset saw nothing. As ADO's
# client cursor does, a filter is now a view over the rows:
#   - clauses "field op value" (=, <>, <, >, <=, >=, LIKE with * or %), joined by AND and OR and grouped
#     with parentheses; text compares without case; 'text', numbers, #dates#, NULL and IS [NOT] NULL
#   - Null matches no comparison; no match: BOF and EOF; adFilterNone or "" brings every row back, in
#     its place; an unknown field is an error
#   - bookmarks stay valid while a filter comes and goes; an array of bookmarks is a filter
#   - a change, a delete and an AddNew under a filter are kept where their rows are
#   - Sort under a filter sorts every row; adFilterPendingRecords shows a batch's changed rows
# The driver is test/adomock-driver.c.
#   WINE=/opt/wine-sg/bin/wine test/adofilter-gate.sh
# Mutant: SG_MUTANT_ADO_FILTER_NONE (recordset.c: the filter shows every row).
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
T=$(mktemp -d /var/tmp/sg-adofilter.XXXXXX)
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
cp "$HERE/adofilter.vbs" "$C/"
timeout -s KILL 60 "$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\SG ADO Mock' /v Driver /d 'C:\adomock.dll' /f >/dev/null 2>&1
timeout -s KILL 60 "$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\ODBC Drivers' /v 'SG ADO Mock' /d Installed /f >/dev/null 2>&1
printf '1\tAnn\t30\n2\tBo\t40\n3\tCy\t50\n4\t\\N\t60\n5\tDee\t25\n6\tEve\t\\N\n' > "$C/people.tsv"
export SG_ADOMOCK_DATA='C:\people.tsv' SG_ADOMOCK_LOG='C:\adomock.log'

out=$(cd "$C" && timeout -s KILL 120 "$WINE" cscript //nologo 'C:\adofilter.vbs' 2>/dev/null </dev/null | tr -d '\r')
[ -n "${VERBOSE:-}" ] && printf '%s\n' "$out"
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got: $(printf '%s' "$out" | grep "^$1=" || echo nothing)"; fi
}
expect rs_open 0 "the recordset opens"
expect all "6:Ann,Bo,Cy,,Dee,Eve" "without a filter: every row"
expect gt "3:Bo,Cy," "Age > 35: the rows it matches, RecordCount theirs"
expect first "Bo,1" "the filter's first row is the current one"
expect or "2:Ann,Cy" "OR, and text compares without case"
expect like "1:Dee" "LIKE 'D*'"
expect group "3:Ann,Bo,Eve" "(a AND b) OR c"
expect and3 "1:Ann" "three clauses with AND, <>"
expect isnull "1:4" "IS NULL"
expect eqnull 1 "= NULL"
expect nullnotless 2 "a Null value matches no comparison"
expect none "0,True,True" "no match: no rows, BOF and EOF"
expect cleared "6:Ann,Bo,Cy,,Dee,Eve:0" "adFilterNone: every row back in its place, Filter reads 0"
expect badfield True "an unknown field is an error"
expect empty "6:0" "an empty string is no filter"
expect bm_filtered "0,Cy" "a bookmark from before the filter finds its row under it"
expect bm_back Cy "and again without the filter"
expect bmarray "2:Ann,Eve" "an array of bookmarks"
expect edit_err 0 "a row changed under a filter is written"
expect edited "Cy,51:Ann,Bo,Cy,,Dee,Eve" "the change is in its row without the filter"
expect delete_err 0 "a row deleted under a filter"
expect deleted "5:Ann,Cy,,Dee,Eve" "is gone, and the others are back"
expect add_err "0,3" "a row added under a filter shows under it"
expect added "6:Ann,Cy,,Dee,Eve,Flo" "and comes after the others without it"
expect sorted_filtered "Flo,,Cy" "Sort under a filter sorts the rows it shows"
expect sorted_all "Flo,,Cy,Ann,Dee,Eve" "and every row is sorted"
expect date_err 0 "a filter with #dates#"
expect date "1:b" "matches the dates between"
expect pending "1:Cy" "adFilterPendingRecords: the row a batch changed"
expect done 1 "the script ran to the end"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
