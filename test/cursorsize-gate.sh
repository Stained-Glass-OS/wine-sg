#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The mouse pointer size through SystemParametersInfo (patches/sg/9811). Windows answers
# SPI_GETCURSORSIZE (0x2028; undocumented, named by Microsoft in Mozilla bug 1712669) with the pointer
# size of Settings > Accessibility > Mouse pointer (HKCU\Control Panel\Cursors CursorBaseSize, 32 by
# default; SG's Settings writes the same value). Firefox and Tor Browser ask it to place tooltips below
# the pointer; Wine answered "Unknown action: 8232" (ERROR_INVALID_SPI_VALUE).
#   default     a new user: TRUE, 32
#   setting     CursorBaseSize 64 (what Settings writes): TRUE, 64
#   WINE=/opt/wine-sg/bin/wine test/cursorsize-gate.sh
# Mutant (win32u/sysparams.c): SG_MUTANT_NO_CURSORSIZE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-cursorsize.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/cursorsize-probe.c" || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
a=$(timeout 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' | sed -n 's/^SIZE //p')
[ "$a" = "1 32 0" ] && pass "a new user: 32" || fail "default: '$a' (want '1 32 0')"
timeout 60 "$WINE" reg add 'HKCU\Control Panel\Cursors' /v CursorBaseSize /t REG_DWORD /d 64 /f >/dev/null 2>&1
b=$(timeout 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' | sed -n 's/^SIZE //p')
[ "$b" = "1 64 0" ] && pass "CursorBaseSize 64 (what Settings writes): 64" || fail "after setting 64: '$b' (want '1 64 0')"
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
