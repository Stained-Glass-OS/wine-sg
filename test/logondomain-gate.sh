#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A local logon's environment (patches/sg/5972): USERDOMAIN (the computer for
# a local account), USERDOMAIN_ROAMINGPROFILE, LOGONSERVER (\\COMPUTER) and
# SESSIONNAME (Console), which Windows puts in the user's Volatile
# Environment at logon. In a shared prefix only the user who ran wineboot had
# one; everyone else's programs had none of them, and %USERDOMAIN%\%USERNAME%
# (icacls, schtasks, installers' ACLs) was "\bob" (qa4, release 621). A value
# the Volatile Environment does set (a domain logon's) still wins.
#   WINE=/opt/wine-sg/bin/wine test/logondomain-gate.sh
# Mutant: SG_MUTANT_NO_LOGON_VARS (dlls/ntdll/unix/env.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-logondomain.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
v() { timeout -s KILL 60 "$WINE" cmd /c "echo %$1%" 2>/dev/null | tr -d '\r'; }
cn=$(v COMPUTERNAME)
[ -n "$cn" ] && [ "$cn" != "%COMPUTERNAME%" ] || { fail "no COMPUTERNAME: '$cn'"; exit 1; }
# a shared prefix's other users: no Volatile Environment at all
"$WINE" reg delete 'HKCU\Volatile Environment' /f >/dev/null 2>&1
[ "$(v USERDOMAIN)" = "$cn" ] && pass "USERDOMAIN is the computer ($cn) with no Volatile Environment" || fail "USERDOMAIN: '$(v USERDOMAIN)' (want $cn; mutant NO_LOGON_VARS)"
[ "$(v USERDOMAIN_ROAMINGPROFILE)" = "$cn" ] && pass "USERDOMAIN_ROAMINGPROFILE too" || fail "USERDOMAIN_ROAMINGPROFILE: '$(v USERDOMAIN_ROAMINGPROFILE)'"
[ "$(v LOGONSERVER)" = "\\\\$cn" ] && pass "LOGONSERVER is \\\\$cn" || fail "LOGONSERVER: '$(v LOGONSERVER)'"
[ "$(v SESSIONNAME)" = "Console" ] && pass "SESSIONNAME is Console" || fail "SESSIONNAME: '$(v SESSIONNAME)'"
# a domain logon's Volatile Environment names the domain: it wins
"$WINE" reg add 'HKCU\Volatile Environment' /v USERDOMAIN /d SGTEST /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Volatile Environment' /v LOGONSERVER /d '\\DC1' /f >/dev/null 2>&1
[ "$(v USERDOMAIN)" = "SGTEST" ] && [ "$(v LOGONSERVER)" = '\\DC1' ] && pass "a domain logon's USERDOMAIN and LOGONSERVER win" \
    || fail "domain logon: USERDOMAIN '$(v USERDOMAIN)' LOGONSERVER '$(v LOGONSERVER)'"
exit $RC
