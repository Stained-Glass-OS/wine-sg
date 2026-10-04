#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A 32-bit installer registering a 64-bit class through HKEY_CLASSES_ROOT
# (patches/sg/0780): the key created with KEY_WOW64_64KEY, its value written
# through that handle, and a subkey created below it, are all in the 64-bit
# view, as on Windows -- the merged view (0178) re-opened the key by its path
# for each value without the flag, so a 32-bit program wrote them into the
# 32-bit view (Wow6432Node) and 64-bit COM found no server: 7-Zip's x64 shell
# extension never loaded (2026-10-03).
#
#   WINE=/opt/wine-sg/bin/wine test/hkcrview-gate.sh   (mutant SG_MUTANT_CLASSES_VIEW_LOST)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$MINGW32" >/dev/null || { echo "SKIP: $MINGW32 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-hkcrview.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW32" -O2 -municode -o "$T/probe32.exe" "$HERE/hkcrview-probe.c" -ladvapi32 -Wl,-e,_wmainCRTStartup 2>/dev/null ||
    "$MINGW32" -O2 -o "$T/probe32.exe" "$HERE/hkcrview-probe.c" -ladvapi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cp "$T/probe32.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && "$WINE" probe32.exe 2>/dev/null | tr -d '\r')
[ "$out" = "32: 0 0 0 0 64: 0 0 0 0" ] && pass "the 32-bit program registers it in both views ($out)" || fail "probe: $out"
K='HKLM\Software\Classes\CLSID\{6A2B9C30-8F1E-4D7A-B3C5-0E9F11A2D7E4}'
q() { "$WINE" reg query "$1" /ve /reg:64 2>/dev/null | tr -d '\r' | awk '/REG_SZ/ { $1 = ""; $2 = ""; sub(/^ +/, ""); print }'; }
[ "$(q "$K")" = "SG view test" ] && pass "its value is in the 64-bit view" || fail "64-bit view: '$(q "$K")'"
[ "$(q "$K\\InprocServer32")" = 'C:\sgview64.dll' ] && pass "...and its subkey's, created below it" || fail "subkey: '$(q "$K\\InprocServer32")'"
W6='HKLM\Software\Classes\Wow6432Node\CLSID\{6A2B9C30-8F1E-4D7A-B3C5-0E9F11A2D7E4}'
[ "$(q "$W6")" = "SG view test 32" ] && [ "$(q "$W6\\InprocServer32")" = 'C:\sgview32.dll' ] \
    && pass "the 32-bit registration stays in the 32-bit view" || fail "32-bit view: '$(q "$W6")' '$(q "$W6\\InprocServer32")'"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
