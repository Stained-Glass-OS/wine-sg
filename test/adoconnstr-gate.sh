#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msado15: a Recordset opened with a connection string (wine-sg 9451). Recordset.Open "SELECT ...",
# "Driver=...;..." -- the way most scripts open one -- failed with adErrInvalidConnection ("Unsupported
# Active connection type 8"), and so did setting ActiveConnection to a string. As ADO does, the string
# opens a Connection of the recordset's own, which is then its ActiveConnection:
#   - Open with a connection string reads the rows; ActiveConnection is an open Connection
#   - a change is written back through that connection
#   - ActiveConnection = "..." before Open; a string that cannot connect is an error, the recordset closed
# The driver is test/adomock-driver.c.
#   WINE=/opt/wine-sg/bin/wine test/adoconnstr-gate.sh
# Mutant: SG_MUTANT_ADO_CONNSTR (recordset.c: a connection string is refused again).
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
T=$(mktemp -d /var/tmp/sg-adoconnstr.XXXXXX)
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
cp "$HERE/adoconnstr.vbs" "$C/"
timeout -s KILL 60 "$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\SG ADO Mock' /v Driver /d 'C:\adomock.dll' /f >/dev/null 2>&1
timeout -s KILL 60 "$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\ODBC Drivers' /v 'SG ADO Mock' /d Installed /f >/dev/null 2>&1
printf '1\tAnn\t30\n2\tBo\t40\n3\tCy\t50\n' > "$C/people.tsv"
export SG_ADOMOCK_DATA='C:\people.tsv' SG_ADOMOCK_LOG='C:\adomock.log'

out=$(cd "$C" && timeout -s KILL 120 "$WINE" cscript //nologo 'C:\adoconnstr.vbs' 2>/dev/null </dev/null | tr -d '\r')
[ -n "${VERBOSE:-}" ] && printf '%s\n' "$out"
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got: $(printf '%s' "$out" | grep "^$1=" || echo nothing)"; fi
}
expect open 0 "Recordset.Open with a connection string"
expect state 1 "the recordset is open"
expect count "3,Ann" "its rows are read"
expect conn_type True "its ActiveConnection is a Connection object"
expect conn_state 1 "which is open"
expect update 0 "a change is written through it"
expect put 0 "ActiveConnection set to a connection string"
expect put_type True "is a Connection object"
expect open2 "0,Ann,31" "Open without a connection uses it, and sees the change written before"
expect bad "True,0" "a string that cannot connect: an error, the recordset stays closed"
expect done 1 "the script ran to the end"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
