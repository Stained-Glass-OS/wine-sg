#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A batch file's lines are read in the console's code page (patches/sg/0756),
# as Windows' cmd does: after "chcp 65001" a UTF-8 script's box drawing and
# symbols were read as the OEM code page -- mojibake (David 2026-10-01: the
# console looked off). A UTF-8 batch file echoes "é─" into a file after
# chcp 65001: the file has those two characters' UTF-8 bytes, not them read
# as code page 437 and written again.
#
#   WINE=/opt/wine-sg/bin/wine test/batchcp-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run (a console of its own)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-batchcp.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
# wineboot ran without a display (scratch-home.sh unsets DISPLAY): let its
# wineserver and desktop go, or the console below inherits a driverless desktop
"$WINESERVER" -w
printf '@echo off\r\nchcp 65001 >nul\r\nchcp >C:\\cp.txt\r\necho \303\251\342\224\200>C:\\out.txt\r\n' > "$WINEPREFIX/drive_c/t.bat"
# in a console of its own: without one, chcp changes nothing
timeout -s KILL 120 xvfb-run -a "$WINE" start /wait cmd /c 'C:\t.bat' < /dev/null > /dev/null 2>&1
"$WINESERVER" -w
grep -q 65001 "$WINEPREFIX/drive_c/cp.txt" 2>/dev/null && pass "chcp 65001 took (the console's code page)" || fail "chcp: $(cat "$WINEPREFIX/drive_c/cp.txt" 2>/dev/null)"
got=$(od -An -tx1 "$WINEPREFIX/drive_c/out.txt" 2>/dev/null | tr -d ' \n')
[ "$got" = "c3a9e294800d0a" ] && pass "after chcp 65001 a UTF-8 batch file's text comes out as written (é─)" \
    || fail "out.txt: ${got:-missing} (expected c3a9e294800d0a)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
