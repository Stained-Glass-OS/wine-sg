#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An account made before the foreground lock (wine-sg 9930) has ForegroundLockTimeout 0 in its registry:
# Wine wrote its old default there at the first session, and the lock would stay off after an upgrade.
# At the next session it is Windows' 200000, once: a 0 the person sets afterwards stays.
#   WINE=/opt/wine-sg/bin/wine test/fglockupgrade-gate.sh   (WINESERVER=... for a build tree)
# Mutant: SG_MUTANT_FG_NO_UPGRADE (win32u).
set -u
unset DISPLAY XAUTHORITY
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fglockupgrade.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
K='HKCU\Control Panel\Desktop'
U='HKCU\Software\Stained Glass\Upgrades'
val() { "$WINE" reg query "$K" /v ForegroundLockTimeout 2>/dev/null | tr -d '\r' | sed -n 's/.*REG_DWORD *//p'; }
session() { "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
v=$(val)
[ "$v" = 0x30d40 ] && pass "a new account has Windows' ForegroundLockTimeout, 200000 ms ($v)" || fail "a new account's ForegroundLockTimeout: $v (want 0x30d40)"
# an account from before the lock: Wine's old 0, and no upgrade made yet
"$WINE" reg add "$K" /v ForegroundLockTimeout /t REG_DWORD /d 0 /f >/dev/null 2>&1
"$WINE" reg delete "$U" /f >/dev/null 2>&1
session
"$WINE" reg query "$K" /v ForegroundLockTimeout >/dev/null 2>&1   # a session's first process
session
v=$(val)
[ "$v" = 0x30d40 ] && pass "an account from before the lock gets 200000 at its next session ($v)" || fail "the old account's ForegroundLockTimeout after a session: $v (want 0x30d40)"
"$WINE" reg add "$K" /v ForegroundLockTimeout /t REG_DWORD /d 0 /f >/dev/null 2>&1
session
"$WINE" reg query "$K" /v ForegroundLockTimeout >/dev/null 2>&1
session
v=$(val)
[ "$v" = 0x0 ] && pass "a 0 set afterwards stays: the upgrade is made once ($v)" || fail "the person's own 0 was changed: $v"
exit $RC
