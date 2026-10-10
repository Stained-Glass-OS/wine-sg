#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# WTSQueryUserToken in a shared prefix (patches/sg/1708): as in a real
# installation, the prefix belongs to the SYSTEM account (SG_SYSTEM, default
# sgsystem) and two standard users (SG_USER1, SG_USER2, default sgtest1 and
# sgtest2, all in SG_GROUP, default sgwine) run programs against its one
# wineserver, each in a session of its own (1706) (test/wtstoken-probe.c):
#  1. SYSTEM (SeTcbPrivilege, as LocalSystem services) gets the primary token
#     of the user logged on to a session: that user's SID, that session
#  2. its own session has no user (ERROR_NO_TOKEN), a session that does not
#     exist is ERROR_CTX_WINSTATION_NOT_FOUND
#  3. a standard user gets ERROR_PRIVILEGE_NOT_HELD, even for its own session
# Exit 77 without the accounts or passwordless sudo.
# Mutant: SG_MUTANT_SESSION_TOKEN_FOR_ALL (server/token.c).
#   WINE=... WINESERVER=... test/wtstoken-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_SYSTEM=${SG_SYSTEM:-sgsystem}
SG_USER1=${SG_USER1:-sgtest1}
SG_USER2=${SG_USER2:-sgtest2}
SG_GROUP=${SG_GROUP:-sgwine}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do
    id "$u" >/dev/null 2>&1 && sudo -n -u "$u" true 2>/dev/null || { echo "SKIP: no $u or no sudo"; exit 77; }
done
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
W=$(mktemp -d /var/tmp/wtstoken.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
# SYSTEM's programs live in the service window station, as sg-wineserver and
# sg-services-start run them: the interactive WinSta0 is the users'
as() {
    local u=$1 ws=; shift
    [ "$u" = "$SG_SYSTEM" ] && ws='__wineservice_winstation\Default'
    sudo -n -u "$u" env ${ws:+SG_WINSTATION="$ws"} WINEPREFIX="$PFX" WINESERVER="$WINESERVER" WINEDEBUG=-all \
        WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME="$W/home-$u" "$@"
}
cleanup() {
    set +e
    as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
    sleep 1
    [ -n "${KEEP:-}" ] || sudo -n rm -rf "$W"
}
trap cleanup EXIT

"$MINGW" -O1 -o "$W/wtstoken-probe.exe" "$HERE/wtstoken-probe.c" -lwtsapi32 -ladvapi32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/wtstoken-probe.exe"
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do sudo -n install -d -o "$u" -m 700 "$W/home-$u"; done
sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$PFX"
sudo -n -u "$SG_SYSTEM" touch "$PFX/.sg-system-prefix"
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
# the machine server restarted as at boot (its first process on the services'
# window station, no display recorded), and the display containers
# sg-session's sg_protect_machine_registry makes at every boot, beneath which
# each user's desktop records its display (as test/dispcfg-gate.sh does)
as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' cmd /c exit" >/dev/null 2>&1
for k in 'HKLM\System\CurrentControlSet\Control\Video' 'HKLM\System\CurrentControlSet\Control\GraphicsDrivers'; do
    as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 120 '$WINE' reg add '$k' /f" >/dev/null 2>&1
done
sudo -n chmod -R g+rwX "$PFX" 2>/dev/null
# no display here: the null graphics driver, so windows can be made
for u in "$SG_USER1" "$SG_USER2"; do
    as "$u" timeout -s KILL 120 "$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f >/dev/null 2>&1
done
sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/wtstoken-probe.exe" "$PFX/drive_c/wtstoken-probe.exe"
# a user's first program sets the user up (profile, desktop): done first,
# for both, so the timing below is that of programs, not of first logons
# (the first attempt may give up)
for u in "$SG_USER1" "$SG_USER2"; do
    for i in 1 2 3; do
        as "$u" timeout -s KILL 300 "$WINE" 'C:\wtstoken-probe.exe' query cur >"$W/warm-$u.out" 2>/dev/null && break
    done
    echo "warm-up $u: $(tr -d '\r' < "$W/warm-$u.out" | grep SESSION)"
done
probe() { local u=$1; shift; as "$u" timeout -s KILL 90 "$WINE" 'C:\wtstoken-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
v() { printf '%s\n' "$1" | sed -n "s/^$2 //p" | head -1; }

as "$SG_USER1" timeout -s KILL 400 "$WINE" 'C:\wtstoken-probe.exe' hold > "$W/hold.out" 2>/dev/null &
for i in $(seq 240); do grep -q HELD "$W/hold.out" 2>/dev/null && break; sleep 1; done
s1=$(v "$(tr -d '\r' < "$W/hold.out")" SESSION)
[ -n "$s1" ] && [ "$s1" != 1 ] && pass "$SG_USER1 is logged on to session $s1" || fail "$SG_USER1's session: $s1"
out=$(probe "$SG_SYSTEM" query "$s1")
q=$(v "$out" QUERY)
case "$q" in
    "1 S-1-5-21-"*" $s1 primary") pass "SYSTEM gets a primary token for session $s1's user: $q";;
    *) fail "SYSTEM, session $s1: $q";;
esac
# the user's SID: the same as the user's own processes have
own=$(as "$SG_USER1" timeout -s KILL 90 "$WINE" cmd /c whoami /user 2>/dev/null | tr -d '\r' | grep -o 'S-1-5-21-[0-9-]*' | head -1)
[ -n "$own" ] && [ "$(printf '%s' "$q" | awk '{print $2}')" = "$own" ] && pass "...it is $SG_USER1's ($own)" \
    || fail "token user $(printf '%s' "$q" | awk '{print $2}'), $SG_USER1 is $own"
[ "$(v "$(probe "$SG_SYSTEM" query 1)" QUERY)" = "0 1008" ] && pass "SYSTEM's own session has no user (ERROR_NO_TOKEN)" \
    || fail "session 1: $(v "$(probe "$SG_SYSTEM" query 1)" QUERY)"
[ "$(v "$(probe "$SG_SYSTEM" query 77)" QUERY)" = "0 7022" ] && pass "a session that does not exist: ERROR_CTX_WINSTATION_NOT_FOUND" \
    || fail "session 77: $(v "$(probe "$SG_SYSTEM" query 77)" QUERY)"
[ "$(v "$(probe "$SG_USER2" query "$s1")" QUERY)" = "0 1314" ] && pass "$SG_USER2 cannot have $SG_USER1's token (ERROR_PRIVILEGE_NOT_HELD)" \
    || fail "$SG_USER2, session $s1: $(v "$(probe "$SG_USER2" query "$s1")" QUERY)"
[ "$(v "$(probe "$SG_USER1" query cur)" QUERY)" = "0 1314" ] && pass "...nor $SG_USER1 its own" \
    || fail "$SG_USER1, its session: $(v "$(probe "$SG_USER1" query cur)" QUERY)"

as "$SG_USER1" "$WINESERVER" -k 2>/dev/null; wait
echo
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL ($fails)"
[ "$fails" = 0 ]
