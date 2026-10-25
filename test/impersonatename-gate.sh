#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# GetUserNameW and LookupAccountSidW on an impersonating thread (6104). GetUserNameW looks up the
# impersonated token's user (1709); LookupAccountSidW named the first user's SID (RID 1000) with
# GetUserNameW -- which, impersonating, looked the same SID up again: the two recursed until the stack
# overflowed. Zoom's MSI custom action (impersonating, connecting to the Task Scheduler, whose Connect
# asks for the user name) died that way and the install failed. Now the RID-1000 name is the process's
# user. The probe impersonates its own user and does the same calls.
#   WINE=/opt/wine-sg/bin/wine test/impersonatename-gate.sh
# Mutant: SG_MUTANT_LOOKUP_RECURSES (advapi32 security.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=x86_64-w64-mingw32-gcc
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v $MINGW >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
T=$(mktemp -d /var/tmp/sg-impersonatename.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=;winedbg.exe=d" WINESERVER
unset DISPLAY XAUTHORITY
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/impersonatename-probe.c" -lole32 -loleaut32 -ladvapi32 -luuid -ltaskschd 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout 90 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
[ "$(v impersonate)" = 1 ] || { fail "the probe could not impersonate"; echo "RESULT: FAIL"; exit 1; }
[ "$(v username)" = 1 ] && [ "$(v same)" = 1 ] && pass "GetUserNameW while impersonating gives the user's name" \
    || fail "GetUserNameW while impersonating: ok=$(v username) same=$(v same) (a stack overflow ends the probe here)"
[ "$(v lookup)" = 1 ] && [ "$(v lookup_same)" = 1 ] && pass "LookupAccountSidW of the token's SID names the same user" \
    || fail "LookupAccountSidW: ok=$(v lookup) same=$(v lookup_same)"
[ "$(v connect)" = 0 ] && [ "$(v done)" = 1 ] && pass "the Task Scheduler connects from the impersonating thread" \
    || fail "ITaskService::Connect: $(v connect), done=$(v done)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
