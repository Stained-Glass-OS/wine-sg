#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CreateEnvironmentBlock: USERPROFILE is the ProfileList's ProfileImagePath,
# and no value is left holding a literal %USERPROFILE% (patches/sg/0470).
#
# A Default User template gives every user TEMP=%USERPROFILE%\AppData\Local\Temp.
# The block a service gets (services.exe builds it for SYSTEM) kept it
# unexpanded -- USERPROFILE was defined only after it was read, and as
# "<profiles>\SYSTEM", not the profile the ProfileList names -- so a service's
# TEMP was "C:\windows\%USERPROFILE%\AppData\Local\Temp": Office's
# Click-to-Run service exited at start and setup failed with 30068-44 (1053).
#
#   WINE=/opt/wine-sg/bin/wine test/envblock-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-envblock.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/envblock-probe.exe" "$HERE/envblock-probe.c" -ladvapi32 -luserenv || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
sid=$("$WINE" "$T/envblock-probe.exe" sid 2>/dev/null | tr -d '\r')
# the template's TEMP, and a profile the ProfileList names (not <profiles>\<name>)
"$WINE" reg add 'HKCU\Environment' /v TEMP /t REG_EXPAND_SZ /d '%USERPROFILE%\AppData\Local\Temp' /f >/dev/null 2>&1
"$WINE" reg delete 'HKCU\Volatile Environment' /f >/dev/null 2>&1
"$WINE" reg add "HKLM\\Software\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList\\$sid" /v ProfileImagePath /d 'C:\users\sgprofile' /f >/dev/null 2>&1
"$WINESERVER" -w
out=$("$WINE" "$T/envblock-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "      $sid" "$out" | sed 's/^/      /'
# (the user's Volatile Environment, which wineboot writes, sets USERPROFILE
# again afterwards; on a Stained Glass system it names the same profile)
printf '%s\n' "$out" | grep -qxF 'TEMP=C:\users\sgprofile\AppData\Local\Temp' \
    && pass "TEMP=%USERPROFILE%\\... is expanded, with the ProfileList's ProfileImagePath" \
    || fail "TEMP: $(printf '%s\n' "$out" | grep TEMP)"
printf '%s\n' "$out" | grep -q '%' && fail "a value still holds a %variable%" || pass "no value holds a literal %variable%"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
