#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msado15/msdasql: ADO Connection.Open passes its user and password through msdasql to
# the ODBC driver (UID=, PWD=); Connection.OpenSchema (tables, columns) from its catalog (6011)
#   WINE=/opt/wine-sg/bin/wine test/adoauth-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-adoauth.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -shared -o "$T/adoauth-driver.dll" "$HERE/adoauth-driver.c" 2>/dev/null &&
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/adoauth-probe.exe" "$HERE/adoauth-probe.c" -lole32 -loleaut32 -luuid 2>/dev/null ||
    { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/adoauth-driver.dll" "$T/prefix/drive_c/adoauth-driver.dll"
timeout -s KILL 60 "$WINE" reg add 'HKLM\Software\ODBC\ODBCINST.INI\SG Mock' /v Driver /d 'C:\adoauth-driver.dll' /f >/dev/null 2>&1
timeout -s KILL 60 "$WINE" reg add 'HKCU\Software\ODBC\ODBC.INI\ODBC Data Sources' /v sgmock /d 'SG Mock' /f >/dev/null 2>&1

export SG_MOCK_ODBC_LOG='C:\mockodbc.txt'
timeout -s KILL 60 "$WINE" "$T/adoauth-probe.exe" 'C:\mockodbc.txt' >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
