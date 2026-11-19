#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# HKCU\Control Panel\International\User Profile (patches/sg/7904): Windows 8 and later keep the
# person's language list there ("Languages", REG_MULTI_SZ, most preferred first, and a key per
# language naming its keyboard), made with the profile. VisualBoyAdvance-M (wxWidgets) showed
# "Can't open registry key 'HKCU\Control Panel\International\User Profile' (error 2)" at every
# start. Checked: a new profile has it, with the user's language and its keyboard; a list the
# person changed is left alone by later processes.
#   WINE=/opt/wine-sg/bin/wine test/userprofile-gate.sh
# Mutant: -DSG_MUTANT_NO_USER_PROFILE (dlls/kernelbase/locale.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-userprofile.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
unset DISPLAY
export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
K='HKCU\Control Panel\International\User Profile'
q() { (cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
v=$(q 'C:\windows\syswow64\reg.exe' query "$K" /v Languages | sed -n 's/^ *Languages *REG_MULTI_SZ *//p')
[ "$v" = "en-US" ] && pass "a new profile lists its language: Languages = en-US (read by a 32-bit process)" ||
    fail "a new profile lists its language (Languages='$v')"
v=$(q reg query "$K\\en-US" /v 0409:00000409 | sed -n 's/^ *0409:00000409 *REG_DWORD *//p')
[ "$v" = "0x1" ] && pass "the language's key names its keyboard (0409:00000409 = 1)" ||
    fail "the language's key names its keyboard ('$v')"
q reg add "$K" /v Languages /t REG_MULTI_SZ /d 'fr-FR\0de-DE' /f >/dev/null
"$WINESERVER" -w
v=$(q reg query "$K" /v Languages | sed -n 's/^ *Languages *REG_MULTI_SZ *//p')
case "$v" in 'fr-FR\0de-DE'*) pass "a list the person changed stays as it is";;
             *) fail "a list the person changed stays as it is ('$v')";; esac
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
