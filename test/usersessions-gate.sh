#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Per-user sessions in a shared prefix (patches/sg/1706): as in a real
# installation, the prefix belongs to the SYSTEM account (SG_SYSTEM, default
# sgsystem) and two standard users (SG_USER1, SG_USER2, default sgtest1 and
# sgtest2, all in SG_GROUP, default sgwine) run programs against its one
# wineserver at once (test/usersessions-probe.c):
#  1. the second user has a session of its own (they shared one)
#  2. it cannot open the first user's Local\ objects, nor find its windows
#     (another window station), but Global\ objects are shared
#  3. the first user's own processes share its session, Local\ and windows
#  4. SYSTEM can start a program in a user's session (SG_SESSION_UID, as
#     sg-elevated-run does); a standard user cannot
# Exit 77 without the accounts or passwordless sudo.
# Mutant: SG_MUTANT_ONE_SESSION (server/process.c).
#   WINE=... WINESERVER=... test/usersessions-gate.sh
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
W=$(mktemp -d /var/tmp/usersessions.XXXXXX)
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

"$MINGW" -O1 -o "$W/usersessions-probe.exe" "$HERE/usersessions-probe.c" -luser32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/usersessions-probe.exe"
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
sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/usersessions-probe.exe" "$PFX/drive_c/usersessions-probe.exe"
# a user's first program sets the user up (profile, desktop): done first,
# for both, so the timing below is that of programs, not of first logons
# (a window, so its desktop and explorer too; the first attempt may give up)
for u in "$SG_USER1" "$SG_USER2"; do
    for i in 1 2 3; do
        as "$u" timeout -s KILL 300 "$WINE" 'C:\usersessions-probe.exe' win >"$W/warm-$u.out" 2>/dev/null && break
    done
    echo "warm-up $u: $(tr -d '\r' < "$W/warm-$u.out" | grep WIN)"
done
probe() { local u=$1; shift; as "$u" timeout -s KILL 90 "$WINE" 'C:\usersessions-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
v() { printf '%s\n' "$1" | sed -n "s/^$2 //p" | head -1; }

as "$SG_USER1" timeout -s KILL 400 "$WINE" 'C:\usersessions-probe.exe' hold one > "$W/hold.out" 2>/dev/null &
for i in $(seq 240); do grep -q HELD "$W/hold.out" 2>/dev/null && break; sleep 1; done
held=$(tr -d '\r' < "$W/hold.out")
s1=$(v "$held" SESSION)
[ "$(v "$held" HELD)" = "1 1 1" ] && pass "$SG_USER1 holds Local\\, Global\\ objects and a window (session $s1)" \
    || fail "$SG_USER1 hold: $held"

out=$(probe "$SG_USER2" look one)
s2=$(v "$out" SESSION)
[ -n "$s2" ] && [ "$s2" != "$s1" ] && pass "$SG_USER2 has a session of its own ($s2, not $s1)" || fail "$SG_USER2's session: $s2 ($SG_USER1: $s1)"
[ "$(v "$out" LOCAL)" = 0 ] && pass "...and cannot open $SG_USER1's Local\\ object" || fail "$SG_USER2 opened $SG_USER1's Local\\ object"
[ "$(v "$out" WINDOW)" = 0 ] && pass "...nor find $SG_USER1's window" || fail "$SG_USER2 found $SG_USER1's window"
[ "$(v "$out" GLOBAL)" = 1 ] && pass "...but Global\\ objects are shared" || fail "$SG_USER2 cannot open a Global\\ object"

out=$(probe "$SG_USER1" look one)
[ "$(v "$out" SESSION)" = "$s1" ] && [ "$(v "$out" LOCAL)" = 1 ] && [ "$(v "$out" WINDOW)" = 1 ] \
    && pass "$SG_USER1's own processes share its session, Local\\ and windows" || fail "$SG_USER1 again: $out"

uid2=$(id -u "$SG_USER2")
# through cmd: the program cmd starts stays in the session cmd was put in
out=$(as "$SG_SYSTEM" env SG_SESSION_UID="$uid2" timeout -s KILL 90 "$WINE" cmd /c 'C:\usersessions-probe.exe' id 2>/dev/null | tr -d '\r')
[ "$(v "$out" SESSION)" = "$s2" ] && pass "SYSTEM starts a program in $SG_USER2's session (SG_SESSION_UID), and its child stays there" \
    || fail "SYSTEM with SG_SESSION_UID: session $(v "$out" SESSION), not $s2"
out=$(as "$SG_USER1" env SG_SESSION_UID="$uid2" timeout -s KILL 90 "$WINE" 'C:\usersessions-probe.exe' id 2>/dev/null | tr -d '\r')
[ "$(v "$out" SESSION)" = "$s1" ] && pass "...a standard user cannot (stays in $s1)" \
    || fail "$SG_USER1 moved to session $(v "$out" SESSION)"

as "$SG_USER1" "$WINESERVER" -k 2>/dev/null; wait
echo
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL ($fails)"
[ "$fails" = 0 ]
